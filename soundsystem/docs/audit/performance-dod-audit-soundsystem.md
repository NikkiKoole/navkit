# Soundsystem DOD / Performance Audit (Casey Muratori Style)

**Date**: 2026-03-11
**Scope**: `soundsystem/` — synth engine, drums, effects, sequencer, sampler, DAW/demo

**Total static memory**: ~3.3MB (excluding loaded samples)

---

## CRITICAL (10x+ performance impact)

### 1. Voice struct is ~10KB — 32 of them = 320KB of mostly cold data

**File**: `synth.h:340-517`

The `Voice` struct packs every synthesis model's state into one union-less mega-struct:

| Field group | Size | Hot? |
|---|---|---|
| Core (phase, freq, env, velocity) | ~60B | YES — every sample |
| Filter state (LP, HP, LFO) | ~40B | YES — every sample |
| LFOs (4 types) | ~80B | YES — every sample |
| `ksBuffer[2048]` (Karplus-Strong) | **8,192B** | Only if WAVE_PLUCK |
| `AdditiveSettings` (16 harmonics × 4 arrays) | ~300B | Only if WAVE_ADDITIVE |
| `GranularSettings` (32 grains × 28B + params) | ~1,000B | Only if WAVE_GRANULAR |
| `VoiceSettings` (formant filters, consonant) | ~120B | Only if WAVE_VOICE |
| `MalletSettings` / `MembraneSettings` | ~180B | Only if those wave types |
| Arp state | ~60B | Only if arp enabled |
| Retrigger/burst | ~60B | Only if retrigger |
| Extra oscillators (osc2-osc6) | ~60B | Only if drum-style |
| Noise layer, click, drive | ~60B | Mixed |

**The problem**: The audio callback runs `processVoice()` 32 times per sample (1,411,200 times/sec at 44.1kHz). Each call touches ~100B of hot data but loads ~10KB into cache because the fields are interleaved with synthesis-specific cold data. That's a **~100x waste ratio**.

The `ksBuffer[2048]` alone is 8KB embedded in every Voice, even when the voice is playing a simple square wave. That's 8KB of dead weight per cache line fetch for 31 out of 32 potential voices.

**Fix**: Hot/cold split.

```c
// ~100B — fits in 2 cache lines, iterated 44,100×/sec per active voice
typedef struct {
    float phase, frequency, baseFrequency;
    float envStage, envTime, envValue;
    float filterLp, filterHp, filterCutoff, filterReso;
    float velocity, pan;
    int waveType;
    bool active;
    // ... core oscillator + filter state
} VoiceHot;

// ~10KB — only accessed when voice is active AND matches wave type
typedef struct {
    float ksBuffer[2048];
    AdditiveSettings additive;
    GranularSettings granular;
    VoiceSettings voice;
    MalletSettings mallet;
    MembraneSettings membrane;
    BirdSettings bird;
    // ... arp, retrigger, extra oscs
} VoiceCold;

VoiceHot voiceHot[NUM_VOICES];   // 3.2KB — fits in L1
VoiceCold voiceCold[NUM_VOICES]; // 320KB — only touched per-voice when needed
```

**Estimated speedup**: 5-20x for the voice processing loop depending on active voice count and wave types. The hot array would fit entirely in L1 cache (3.2KB vs current 320KB).

---

### 2. Bus delay buffers: 1.2MB of scattered random access

**File**: `effects.h:167` — `float busDelayBuf[BUS_DELAY_SIZE]` per `BusState`, where `BUS_DELAY_SIZE = SAMPLE_RATE = 44,100`

7 buses × 44,100 floats × 4 bytes = **1.23MB** of delay buffers inside `BusState`.

Each bus reads from `busDelayBuf[readPos]` and writes to `busDelayBuf[writePos]` per sample. The read position is typically hundreds to thousands of samples behind the write position (depending on delay time). This means every bus delay read is a **guaranteed L1 miss** — it's reading from a position that was written ~10-1000ms ago, which is 440-44,100 samples away.

With 7 buses, that's 7 L1 misses per sample = **308,700 cache misses per second** just for bus delays.

**Fix**: Change inline array to pointer, allocate on enable, free on disable.

**Step 1** — Change `BusState` (effects.h:158):
```c
typedef struct {
    float filterIc1eq;
    float filterIc2eq;
    float distFilterLp;

    float *busDelayBuf;       // NULL when delay off, calloc'd when on
    int busDelayWritePos;
    float busDelayFilterLp;
} BusState;
```

**Step 2** — Allocate/free in `setBusDelay` (effects.h:1801):
```c
static void setBusDelay(int bus, bool enabled, float time, float feedback, float mix) {
    _ensureMixerCtx();
    if (bus < 0 || bus >= NUM_BUSES) return;

    BusEffects *b = &mixerCtx->bus[bus];
    BusState *s = &mixerCtx->busState[bus];

    if (enabled && !s->busDelayBuf) {
        s->busDelayBuf = (float *)calloc(BUS_DELAY_SIZE, sizeof(float));
        s->busDelayWritePos = 0;
        s->busDelayFilterLp = 0.0f;
    } else if (!enabled && s->busDelayBuf) {
        free(s->busDelayBuf);
        s->busDelayBuf = NULL;
    }

    b->delayEnabled = enabled;
    b->delayTime = time;
    b->delayFeedback = feedback;
    b->delayMix = mix;
}
```

**Step 3** — Add pointer guard in `processBusEffects` delay block (effects.h:1633):
```c
    if (bus->delayEnabled && state->busDelayBuf) {
```

**Step 4** — Song/DAW file load paths: after restoring `BusEffects` params, loop over buses and materialize buffers for any that have `delayEnabled == true`:
```c
for (int i = 0; i < NUM_BUSES; i++) {
    if (mixerCtx->bus[i].delayEnabled && !mixerCtx->busState[i].busDelayBuf) {
        mixerCtx->busState[i].busDelayBuf = (float *)calloc(BUS_DELAY_SIZE, sizeof(float));
    }
}
```

**No other changes needed**: `initMixerContext` already `memset`s to 0 (NULL pointers), and the `delayEnabled` guard at line 1633 already prevents access when off.

**Estimated improvement**: MixerContext drops from ~1.24MB to ~700 bytes. 2-7x reduction in cache pressure depending on active delay count.

---

## HIGH (2-10x impact)

### 3. Effects context: 1.6MB of buffers, most cold at any given time

**File**: `effects.h:362-400`

| Buffer | Size | Access pattern |
|---|---|---|
| `delayBuffer[88,200]` | 352KB | Random read (delay time offset) |
| `dubLoopBuffer[176,400]` | 706KB | Random read (multi-head tape) |
| `rewindBuffer[132,300]` | 529KB | Sequential write, variable-speed read |
| Reverb combs (4 buffers) | 24KB | Sequential (good!) |
| Reverb allpass (2 buffers) | 3KB | Sequential (good!) |
| Chorus/flanger | 10KB | Small offset reads (OK) |

The delay, dub loop, and rewind buffers are **always allocated** even when disabled. That's 1.6MB of resident memory competing for cache even when the user just wants dry output.

**Fix**: Pointer-based lazy allocation. `float *delayBuffer` initialized to NULL, malloc on first enable, free on disable. Zero cost when off.

```c
typedef struct {
    float *delayBuffer;      // NULL when delay off, malloc'd when on
    float *dubLoopBuffer;    // NULL when loop off
    float *rewindBuffer;     // NULL when rewind off
    // ... reverb stays embedded (always-on, small)
} EffectsContext;
```

**Estimated improvement**: 3-5x less cache pressure when effects are partially disabled (common case).

---

### 4. Pattern struct: v1 + v2 data duplication = ~24KB per pattern

**File**: `sequencer.h:284-319`

During the v1→v2 transition, every Pattern contains BOTH representations:

| Data | Size |
|---|---|
| v1 drum arrays (steps, velocity, pitch, prob, cond) | ~2.2KB |
| v1 melody arrays (note, vel, gate, prob, cond, slide, accent, sustain) | ~2.5KB |
| v2 StepV2[12][32] (46 bytes × 384) | **17.7KB** |
| PLocks + index | ~1.3KB |
| Overrides | ~60B |
| **Total per pattern** | **~24KB** |

8 patterns = **192KB**. The v1 data is ~4.7KB per pattern of pure duplication (already represented in v2). Every `patSet*` function dual-writes to both v1 and v2 (see the many `// v2 dual-write` comments), doubling write bandwidth.

**Fix**: Complete the v2 migration (Phase 4 per `sequencer-v2-plan.md`) and delete all v1 arrays. This halves pattern memory and eliminates dual-write overhead.

**Estimated improvement**: 2x less memory, 2x less write bandwidth in step editing paths.

---

### 5. Reverb comb filters: prime-sized buffers prevent prefetch optimization

**File**: `effects.h:366-369`

```c
#define REVERB_COMB_1 1557
#define REVERB_COMB_2 1617
#define REVERB_COMB_3 1491
#define REVERB_COMB_4 1422
```

Prime-sized circular buffers mean modulo operations compile to actual division (not bitwise AND). At 44,100 samples/sec × 4 combs = **176,400 modulo operations per second**.

The 4 combs are processed in parallel (read all 4, sum, write all 4). Each comb reads from a different offset, causing 4 cache line fetches per sample that can't be predicted or prefetched.

**Fix**: Round up to nearest power-of-2 (2048 for all four). Use bitmask instead of modulo. Slight change in reverb character (compensate with feedback tuning).

```c
#define REVERB_COMB_SIZE 2048  // Power of 2, bitmask = 0x7FF
// readPos = (writePos - delay) & 0x7FF;
```

**Estimated improvement**: 2-3x for reverb processing (eliminate division, improve prefetch).

---

## MEDIUM (measurable but modest)

### 6. Granular: always iterating 32 grain slots even when most are inactive

**File**: `synth.h:1871`

```c
for (int i = 0; i < GRANULAR_MAX_GRAINS; i++) {
    Grain *g = &gs->grains[i];
    if (!g->active) continue;
    // ... process grain
}
```

With typical density (5-15 grains active), this checks 17-27 inactive grains per sample. Each Grain is ~28 bytes, so the full scan touches ~900B.

**Fix**: Maintain an active grain count or use a packed active list. When `nextGrain` wraps, compact active grains to front.

**Estimated improvement**: Minor (900B isn't terrible), but cleaner.

### 7. sinf() calls in hot audio paths

**Files**: `synth.h`, `drums.h` — multiple `sinf()` calls per voice per sample

Phase accumulation → `sinf(phase * 2π)` is the core of most oscillators. `sinf()` is expensive (~20-50 cycles depending on implementation). With 32 voices × potentially multiple oscillators (osc2-osc6), this can be 100+ `sinf()` calls per sample.

**Fix**: Fast sine approximation (polynomial or lookup table). A 4th-order polynomial sine is ~4 cycles and sufficient for audio:

```c
static inline float fastSin(float x) {
    // Normalize to -π..π, then Bhaskara or minimax polynomial
    x = x - (int)(x * (1.0f / (2*PI))) * (2*PI);
    float x2 = x * x;
    return x * (1.0f - x2 * (0.16666f - x2 * 0.00833f));
}
```

**Estimated improvement**: 2-5x for sine-heavy patches (additive with 16 harmonics, FM, multi-oscillator drums).

**Experiment (2026-03-17)**: Created `fast_math.h` with drop-in replacements for sinf/cosf/tanf/tanhf/expf/powf (polynomial/rational approx). Replaced all 133 calls across synth.h, synth_oscillators.h, effects.h, dub_loop.h. Build succeeded but 6 tests failed:
- `unison_detune` (5 failures): `fast_powf(2.0f, cents/1200.0f)` not precise enough for exact cent-to-ratio comparisons at 0.001 tolerance
- `bitcrusher_effect` (1 failure): `fast_powf(2.0f, bits)` quantization level mismatch at FLOAT_EPSILON tolerance

**Recommended selective approach** (next attempt):

Safe to replace (not pitch-sensitive, inherently approximate):
- `tanf` → `fast_tanf` in SVF filter coefficient calc (synth.h:2112, effects.h:1724) — **highest win**, runs 40×/sample
- `tanhf` → `fast_tanhf` for drive/saturation (synth.h, effects.h, dub_loop.h) — waveshaping is approximate by nature
- `sinf`/`cosf` → `fast_sinf`/`fast_cosf` for LFOs, chorus/phaser modulation, stereo pan — low-frequency signals, error inaudible

Keep as libm (precision matters):
- `powf(2.0f, x/12.0f)` — pitch/detune/semitone math. Causes audible tuning drift and test failures
- `powf(2.0f, bits)` — bitcrusher quantization levels. Exact integer powers needed
- `powf(10.0f, dB/20.0f)` — EQ/compressor gain. Precision affects loudness balance
- `expf` in envelope decay — exact timing for ADSR shapes, user noticed audible difference

Reverted for now.

### 8. Function pointers for drum/melody trigger callbacks

**File**: `sequencer.h` — 7 function pointer callbacks (4 drum + 3 melody) called per step trigger

```c
seq.drumTrigger[track](drumType, velocity, pitch);
seq.melodyTrigger[track](note, velocity, gate);
```

Indirect calls (function pointer dispatch) defeat branch prediction. With 7 tracks and potentially rapid triggers (16th notes at 180 BPM = ~12 triggers/sec per track), this is ~84 mispredicted branches/sec.

**Impact**: Low in absolute terms (84/sec is nothing vs 44,100 audio samples/sec), but these are in the sequencer tick path which should be as tight as possible.

**Fix**: Not urgent. A switch on track index would be marginally faster but less flexible.

---

## LOW (minor or cold-path only)

### 9. SynthPatch is ~600B but only accessed on note-on or param change
Not a hot path issue. Patches are read when triggering notes or changing UI params. Current layout is fine.

### 10. Sampler malloc on WAV load
`samplerLoadWav()` does one malloc per sample load. This is file I/O path, not audio path. Acceptable.

### 11. DAW recording buffer malloc
`daw.c:236` — malloc on F7 press for recording buffer. One-time allocation, not per-frame. Fine.

### 12. `memset` on pattern change
Multiple `memset(seq.trackStepPlayCount, ...)` calls on pattern switch. These are small arrays (7×32 ints = 896 bytes) and only happen on pattern change (rare). Fine.

---

## ALREADY GOOD

- **Drum voice processing**: Clean switch dispatch by drum type, no function pointers. Direct `sinf()` + `expDecay()` per voice. Compact DrumVoice struct (~80B).
- **Static allocation everywhere**: No malloc in audio callback. All voices, patterns, effects contexts are static globals or stack-local. This is correct.
- **Circular buffer pattern**: Delay lines, reverb combs, chorus/flanger all use modular ring buffers with read/write pointers. Memory-efficient, no allocation.
- **P-lock linked list**: `nextInStep` chaining avoids per-step scanning of all 128 p-locks. Only iterate locks that apply to the current step. Good sparse access pattern.
- **StepV2 packing**: 46 bytes per step with uint8_t fields and compact StepNote (7 bytes). Well-packed for cache.
- **Embedded SCW data**: Optional compile-time embedding (`scw_data.h`) avoids runtime file I/O. Good for embedded/game use.
- **DrumsContext is compact**: 16 voices × 80B = 1.3KB. Fits in L1. Hot path (processDrumVoice) touches ~30B per voice. Clean.
- **Sequencer tick is efficient**: 96 PPQ tick loop checks per-track counters, only triggers on exact tick match. No wasted iteration.

---

## IMPACT SUMMARY TABLE

| # | Issue | Memory/Cache Impact | Effort | Estimated Speedup |
|---|-------|-------------------|--------|-------------------|
| 1 | Voice struct hot/cold split (10KB → 100B hot) | 320KB wasted in L1/L2 | Medium | 5-20x voice loop |
| 2 | Bus delay buffers always allocated (1.2MB) | 1.2MB cache pressure | Low | 2-7x cache reduction |
| 3 | Effects buffers always allocated (1.6MB) | 1.6MB when effects off | Low | 3-5x when effects off |
| 4 | Pattern v1+v2 duplication (17.7KB wasted/pat) | 142KB wasted, 2x writes | Medium (finish v2 migration) | 2x pattern memory |
| 5 | Prime-sized reverb combs (modulo division) | 176K divisions/sec | Low | 2-3x reverb |
| 6 | Granular 32-slot scan | 900B per sample | Low | Minor |
| 7 | sinf() in hot audio paths | CPU cycles | Medium | 2-5x sine-heavy patches |
| 8 | Sequencer function pointer dispatch | Branch misprediction | Low | Negligible |

**Priority order**: #1 (Voice split) >> #4 (v2 migration) > #7 (fast sine) > #5 (reverb P2) > #2/#3 (lazy buffers) > #6 > #8

---

## THREAD SAFETY: Audio Callback Runs on a Separate Thread

### Current situation

Raylib's `SetAudioStreamCallback` runs the callback (`DawAudioCallback` in `daw.c:3104`, `SynthCallback` in `demo.c`) on **miniaudio's dedicated audio thread** — not the main/render thread. This means the audio callback and UI code run concurrently with **zero synchronization**.

The UI thread writes shared state (synth voices, sequencer, effects params, patches) while the audio thread reads those same fields every sample. There are no mutexes, atomics, or message queues.

### Why it works (mostly)

For single aligned floats and bools on x86/ARM, stores are effectively atomic — the audio thread sees either the old value or the new value, never a torn half-write. Most parameter changes (cutoff, volume, pitch) are single floats, so this is fine in practice.

### Where it can break

Multi-field updates can be observed in a half-written state. For example, `setBusDelay(bus, true, 0.3f, 0.5f, 0.4f)` writes `delayEnabled`, `delayTime`, `delayFeedback`, and `delayMix` as four separate stores. The audio thread could see `delayEnabled=true` with stale values for time/feedback/mix for one buffer (~256 samples = ~6ms). Similarly, loading a full SynthPatch or switching patterns writes hundreds of fields that the audio thread could read mid-update.

This manifests as occasional subtle glitches — a single buffer with wrong delay time, a note triggered with stale patch params. Rarely audible, but possible.

### Raylib provides no helpers for this

Raylib is intentionally minimal — `SetAudioStreamCallback` is the entire audio threading API. Synchronization is the user's responsibility. Raylib uses a mutex internally for its own buffer management, but that's not exposed and you wouldn't want a mutex in an audio callback anyway (priority inversion = audio glitches).

### Fix: Lock-free parameter queue (if needed)

The standard audio app pattern is a single-producer single-consumer (SPSC) lock-free ring buffer for UI → audio thread messages:

```c
#define PARAM_QUEUE_SIZE 256

typedef struct {
    int target;      // e.g. bus index, voice index
    int param;       // which parameter
    float value;     // new value
} ParamChange;

typedef struct {
    ParamChange items[PARAM_QUEUE_SIZE];
    _Atomic int head;  // written by UI thread only
    _Atomic int tail;  // read by audio thread only
} ParamQueue;

// UI thread pushes param changes:
static inline void paramQueuePush(ParamQueue *q, int target, int param, float value) {
    int next = (q->head + 1) % PARAM_QUEUE_SIZE;
    if (next == q->tail) return;  // full — drop (UI is way ahead of audio)
    q->items[q->head] = (ParamChange){target, param, value};
    q->head = next;  // atomic store publishes the entry
}

// Audio callback drains at start of each buffer:
static inline void paramQueueDrain(ParamQueue *q) {
    while (q->tail != q->head) {
        ParamChange *c = &q->items[q->tail];
        applyParam(c->target, c->param, c->value);
        q->tail = (q->tail + 1) % PARAM_QUEUE_SIZE;
    }
}
```

The audio callback calls `paramQueueDrain()` once at the top of each buffer fill. All param writes are then applied atomically from the audio thread's perspective — no torn multi-field updates.

### Priority: LOW

The current "just write it" approach works for this use case. Single-float params are the vast majority of writes and are safe. The risk is limited to multi-field batch updates (pattern load, patch swap, bus config) which happen rarely and produce at worst a single ~6ms glitch. Worth knowing about, not worth fixing until it becomes audible.
