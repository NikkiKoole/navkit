# Plan of Attack — Remaining Work

Single source of truth for open items, prioritized by impact. Specs and historical notes live in their own docs; when items are complete, mark the doc as ready to move to `done/` and keep this list current.

Instruments and effects are considered complete — 29 engines, 263 presets, comprehensive effect chain. Focus is now on architecture, workflow, performance, and testing.

## Current State (Verified)
- 29 synthesis engines
- 263 instrument presets
- 32-voice polyphony + 8-voice sampler
- 12 sequencer tracks, 64 patterns, up to 32 steps
- 8 buses (4 drum + 3 melodic + sampler)
- DAW tabs: Sequencer, Piano Roll, Song, MIDI, Sample, Voice, Arrange
- Formant/vocoder phases 1-2 done (per-voice + p-lock)

## Priority 1 — High Impact, Do Next

### 1. Scenes + Crossfader System
- **Spec:** `scene-crossfader-spec.md`
- **Effort:** Medium (~750 LOC)
- **Prerequisite:** Live params Phase 1-2 (#3) — crossfader writes to patch `p_` fields, voices must re-read them per-sample for smooth morphing on held notes
- **What:** Complete snapshot storage (8 slots) of all sound state (patches, effects, mixer, BPM) with smooth morphing via 0-1 crossfader
- **State:** UI shell exists (Song tab scene buttons, Mix tab XFade controls) but no backing logic — scenes never save/load, crossfader never blends
- **Why first:** Linchpin for adaptive game audio (calm→combat morphing). Blocks the C conductor and crossfader automation. Everything downstream depends on this
- **Counterpoint:** The game-side interactive music doc (`docs/doing/interactive-music-system.md`) argues the crossfader may not be needed — pattern switching + track volume fading + dub loop transitions cover ~90% of the use cases. That doc proposes a simpler **Music Director** (mood-driven song/pattern selection + vertical layering) as the primary approach, with scenes/crossfader only if that feels too coarse. See also `docs/todo/ensemble-stations.md` for diegetic music (movers playing instruments at stations)

### ~~2. Test Coverage (3 Critical Gaps)~~ — DONE
All three gaps filled: patch trigger (8+ suites, 140+ fields), rhythm patterns (all 14 styles, 5 variations, prob maps, euclidean), synth oscillators (10 describe blocks covering all major wave types). See `done/test-gaps-audit-soundsystem.md`. Remaining minor gaps: sampler WAV loading (deferred until sampler improvements), effects edge cases (low risk).

### 3. Live Parameters + Mod Matrix
- **Spec:** `live-params-and-mod-matrix.md` (unified doc, supersedes old separate specs)
- **Effort:** Medium-large (~1100-1250 LOC across 7 phases)
- **What:** Phase 1-2: patch pointer on Voice + live param reads (knob tweaks affect held notes). Phase 3-7: mod matrix with flexible source→destination routing (velocity, note number, LFOs, envelope → any param)
- **Pain point:** Filterbank, filter cutoff, wavefold — all frozen at note-on currently
- **Bonus:** MIDI CC gets live params for free. Also prerequisite for crossfader (#1) to work on held notes
- **Key insight:** Phase 1-2 (live params, ~250-350 LOC) unlocks both crossfader and mod matrix. Phase 2 is the most labor-intensive — touches 50-100 read sites across every oscillator engine

## Priority 2 — Good Value, Can Wait

### 4. Preset Cleanup
- **Effort:** Low (decisions + deletions, ~1 hour)
- **Source:** `preset-audit.md` (regenerate with `preset-audition --audit`)
- **Actions:**
  - Remove [54] Mel Tabla (exact duplicate of [96] Tabla)
  - Rename 7 name collisions (Glockenspiel, Xylophone, PD Bass, PD Lead, Tambourine, Guiro, Cabasa)
  - Fix [167] Moog Sci-Fi clipping (peak=1.145)
  - Fix or remove 3 silent presets ([102] Vinyl Crackle, [121] Grain Pad, [122] Grain Shimmer)
  - Address 13 presets with high DC offset

### 5. Pure C Conductor (Game Integration)
- **Effort:** Small-medium (~200-400 LOC)
- **Depends on:** Bridge API additions (track volume, pattern queue). Scenes/crossfader optional — see counterpoint in #1. Full Lua version: `lua-conductor-architecture.md`
- **What:** Map game state (fire danger, time of day, job count) → song selection, pattern switching, and track volume fading
- **Full spec:** `docs/doing/interactive-music-system.md` — 7-step implementation plan covering music director, SFX system, beat-synced events, ambient layers, and composition guidelines
- **Why not Lua:** Pure C avoids new dependency, sufficient for colony sim logic. Full Lua conductor (`lua-conductor-architecture.md`) deferred until this proves value

### ~~6. Mod Matrix~~ — Merged into #3
Now part of `live-params-and-mod-matrix.md` (Phases 3-7). Shares the same foundation (patch pointer on Voice) as live params.

### 7. Edit-in-Context
- **Effort:** Small (~50 LOC)
- **What:** Click arrangement cell → jump to Sequencer tab with that pattern loaded and track selected
- **Part of:** Flexible track architecture vision (see Deferred section) but standalone

## Priority 3 — Performance (When It Hurts)

### 8. Voice Hot/Cold Split
- **Effort:** Medium, medium risk
- **What:** Voice struct is ~10KB; hot path only needs ~100B. Split into `VoiceHot[32]` (3.2KB, L1-friendly) and `VoiceCold[32]` (synthesis-specific buffers)
- **Value:** 5-20x speedup for voice processing loop (300KB → 3.2KB working set)
- **Risk:** Subtle bugs from field segregation. Needs careful testing

### 9. Lazy Buffer Allocation
- **Effort:** Low (~150 LOC total, low risk)
- **What:** Delay/dub-loop/rewind buffers (2.8MB) always allocated. Change to pointer-based: malloc on enable, free on disable
- **Value:** 3-5x cache pressure reduction when effects partially disabled (common case)

### 10. Fast Sine Approximation
- **Effort:** Medium (requires care)
- **What:** Replace libm `sinf()` with 4th-order polynomial (~4 cycles vs 20-50). ~133 call sites
- **Gotcha:** Previous attempt broke 6 tests (pitch-sensitive paths). Recommended: LFO/modulation only, keep libm for pitch calculations

### 11. Reverb Comb Power-of-2
- **Effort:** Low (~50 LOC)
- **What:** Replace prime-sized comb buffers with power-of-2, use bitmask wrap instead of modulo
- **Value:** 2-3x for reverb processing. Minor absolute impact

## Deferred — Not Yet Needed

### Flexible Track Architecture
Three-phase vision for per-track patterns. Details in `done/todo-daw-ideas.md` ("Flexible Track/Pattern Architecture").
1. **Edit-in-context** (Priority 2 above, standalone)
2. **Focused track editing** — dim other tracks, "duplicate pattern for this track" forks one track to a new pattern
3. **Single-track patterns / clips** — patterns hold one track only, arrangement maps `(clip, track)` pairs. Two directions: (a) single-track clips, (b) flexible track types (any slot = drum/melody/sampler). Option (b) also resolves the open `TRACK_DRUM` vs `TRACK_MELODIC` unification question from `done/unified-synth-drums.md`

**Signal to start:** pattern reuse feels limiting, or editing one track keeps disrupting others.

### Other Deferred Items
- **Automation lanes** — explicitly dropped; scenes own long sweeps, p-locks stay discrete
- **Lua conductor** — overkill until pure C conductor proves value
- **Arrangement horizontal scroll** — only matters when songs exceed 14 sections
- **Multi-step selection** — workflow polish, post-core
- **WAV export for samples** — useful but not blocking; defer until sample authoring workflow matures
- **Record launcher → arrangement** — phase 3 of clip launcher, deferred
- **Formant phase 3 (text→phoneme)** — parseTextToPhonemes() partially designed, wire into formant p-locks when desired
- **Preset name editing** — UI polish, low priority
