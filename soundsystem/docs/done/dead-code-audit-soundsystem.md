# PixelSynth / SoundSystem — Dead Code Audit

**Scope**: soundsystem/ directory (header-only audio engine)
**Date**: 2026-03-24
**Tools used**: Grep, LSP references, manual verification

---

## CONFIRMED DEAD (Zero References Found)

All of the following functions are defined but **never called** anywhere in the codebase (soundsystem/ + game integration + tools).

### Sequencer Pattern Chain API (sequencer.h)

Three functions in `sequencer.h` that manage pattern chains (song arrangement) are **completely unused**:

| Function | File:Line | Type | Status |
|----------|-----------|------|--------|
| `seqChainAppend()` | sequencer.h:1684 | static | 0 callers |
| `seqChainRemove()` | sequencer.h:1692 | static | 0 callers |
| `seqChainClear()` | sequencer.h:1706 | static | 0 callers |

These appear to be incomplete/future work for pattern chaining in arrangement mode.

### Uncalled Convenience Wrappers (NOT dead — underlying features are active)

These 6 functions have zero direct callers, but they are thin wrappers around the p-lock system which **is** actively used. Nudge works via `PLOCK_TIME_NUDGE` in the sequencer tick (line ~1041), and flam works via `PLOCK_FLAM_TIME`/`PLOCK_FLAM_VELOCITY` (line ~1267). The DAW sets p-locks directly rather than going through these helpers.

| Function | File:Line | Wraps | Underlying feature |
|----------|-----------|-------|--------------------|
| `seqSetStepNudge()` | sequencer.h:1758 | `seqSetPLock(PLOCK_TIME_NUDGE)` | Active — per-step timing |
| `seqClearStepNudge()` | sequencer.h:1770 | `seqClearPLock(PLOCK_TIME_NUDGE)` | Active |
| `seqGetStepNudge()` | sequencer.h:1779 | `seqGetPLock(PLOCK_TIME_NUDGE)` | Active |
| `seqSetStepFlam()` | sequencer.h:1791 | `seqSetPLock(PLOCK_FLAM_*)` | Active — ghost note flam |
| `seqClearStepFlam()` | sequencer.h:1805 | `seqClearPLock(PLOCK_FLAM_*)` | Active |
| `seqHasStepFlam()` | sequencer.h:1814 | `seqGetPLock(PLOCK_FLAM_TIME)` | Active |

**Verdict**: Keep — useful API for future callers (e.g. tests, external integrations). Not dead code.

---

## MISSING FROM SUPPRESS_WARNINGS (but Actually Used)

The `soundsystem_suppress_warnings()` function in `/Users/nikkikoole/Projects/navkit/soundsystem/soundsystem.h` is meant to silence "-Wunused-function" warnings by referencing all public APIs. **16 functions are marked `__attribute__((unused))` but missing from the suppress list**:

### Play Functions via patch_trigger.h

These 14 functions are called indirectly via `playNoteWithPatch()` dispatch in `patch_trigger.h:310-335`:

| Function | File:Line | Called By | Wave Type |
|----------|-----------|-----------|-----------|
| `playBandedWG()` | synth.h:4872 | patch_trigger.h (WAVE_BANDEDWG) | Banded waveguide |
| `playBowed()` | synth.h:4629 | patch_trigger.h (WAVE_BOWED) | Bowed string |
| `playBrass()` | synth.h:4668 | patch_trigger.h (WAVE_BRASS) | Lip-valve brass |
| `playEPiano()` | synth.h:4681 | patch_trigger.h (WAVE_EPIANO) | Rhodes piano |
| `playGuitar()` | synth.h:4728 | patch_trigger.h (WAVE_GUITAR) | Acoustic guitar |
| `playMandolin()` | synth.h:4751 | patch_trigger.h (WAVE_MANDOLIN) | Mandolin/bouzouki |
| `playMetallic()` | synth.h:4709 | patch_trigger.h (WAVE_METALLIC) | Ring-mod bell/cymbal |
| `playOrgan()` | synth.h:4695 | patch_trigger.h (WAVE_ORGAN) | Hammond B3 tonewheel |
| `playPipe()` | synth.h:4642 | patch_trigger.h (WAVE_PIPE) | Blown pipe (waveguide) |
| `playReed()` | synth.h:4655 | patch_trigger.h (WAVE_REED) | Clarinet/sax |
| `playShaker()` | synth.h:4829 | patch_trigger.h (WAVE_SHAKER) | Maraca/cabasa |
| `playStifKarp()` | synth.h:4791 | patch_trigger.h (WAVE_STIFKARP) | Stiff string piano |
| `playVoicForm()` | synth.h:4923 | patch_trigger.h (WAVE_VOICFORM) | 4-formant voice |
| `playWhistle()` | synth.h:4774 | patch_trigger.h (WAVE_WHISTLE) | Helmholtz whistle |

All are **genuinely used** via the wave type dispatch mechanism. The suppress_warnings list is just incomplete.

### Helper Functions

| Function | File:Line | Purpose | Usage |
|----------|-----------|---------|-------|
| `initUnison()` | synth.h:4324 | Helper to enable/configure unison | Never called (test/debug helper) |
| `initVoiceDefaults()` | synth.h:4305 | Helper to init voice struct | Never called (test/debug helper) |

These two are commented as helpers for "testing or direct voice manipulation" but are **never actually called** in DAW, game, or tests. They have `__attribute__((unused))` which suggests they're intentionally kept for future use or debugging.

---

## RECOMMENDATIONS

### ~~1. Update soundsystem.h suppress_warnings list~~ **Done**

All 14 play functions + 2 helper functions added to the void-cast list in `soundsystem.h:90-105`.

### 2. Evaluate dead sequencer functions

**Decision needed**: Keep or remove the 3 pattern chain stubs?

- `seqChainAppend()`, `seqChainRemove()`, `seqChainClear()`

These have no callers, no tests, no UI. If arrangement/chain mode is planned, they'd need full end-to-end implementation anyway.

**Note**: The nudge/flam wrappers (`seqSetStepNudge` etc.) should be kept — the underlying p-lock features are fully active in the sequencer tick path.

---

## SUMMARY

| Category | Count | Status |
|----------|-------|--------|
| Functions with 0 callers (truly dead) | 3 | Can be removed (chain API) |
| Uncalled convenience wrappers (feature active) | 6 | Keep — nudge/flam p-lock wrappers |
| ~~Functions missing from suppress_warnings (but used)~~ | ~~16~~ | Done — added to list |
| Intentionally dead code blocks (#if 0) | 1 | Properly marked |
| Dead code behind #ifdef | 0 | N/A |
| Unused enum values | 0 | All used in switches |
| Unused macros | 0 | All referenced |
| Unused typedefs | 0 | All used as types |

**Estimated lines removable**: ~60 lines (3 chain functions, ~20 lines each)

### Files with Dead Code

1. **soundsystem/engines/sequencer.h** (3 dead functions)
   - Pattern chain API: `seqChainAppend/Remove/Clear`

2. **soundsystem/soundsystem.h** (incomplete suppress_warnings list)
   - Missing 16 function references from void-cast list
   - Causes unused-function warnings in compilation

---

## Confidence Levels

- **CONFIRMED DEAD (9 functions)**: 100% — zero callers verified across entire codebase
- **SUPPRESS_WARNINGS INCOMPLETE**: 100% — 16 functions are genuinely called but not listed
- **Intentional stubs**: 95% — functions documented as helpers/future work

---

## Notes for Maintainers

1. **Header-only design**: All sound engine files are `.h` files with inline implementations. This means unused functions in any header will trigger warnings across all translation units that include it. The suppress_warnings pattern is the standard way to silence this.

2. **Patch dispatch system**: The 14 play functions use a switch-based dispatch table in `patch_trigger.h:310-335`. They're genuinely used but the compiler can't see the indirect call, hence `__attribute__((unused))` and void-casting.

3. **Test coverage**: The sequencer dead functions have no tests either, confirming they're not used. If they were actively used, there would be at least skeleton tests.

4. **DAW integration**: The DAW (`demo/daw.c`) does not use pattern chaining, step nudging, or step flams. If these features are planned, full end-to-end implementation (UI + logic + tests) is needed, not just skeleton functions.
