# Bounce Fidelity Bug — RESOLVED (2026-03-16)

## Summary

GUI DAW bounce produced different audio from CLI/headless, and was nondeterministic across repeated bounces. CLI and headless `--bounce` mode were always correct and deterministic.

## Root Cause

**`dawLoad`'s re-bounce mechanism triggered a recursive `chopStateBounce` inside every GUI bounce.**

The `#ifdef DAW_HAS_CHOP_STATE` guard in `sample_chop.h` was supposed to clear `chopState.sourcePath` before `dawLoad` to prevent re-entry. But `DAW_HAS_CHOP_STATE` is defined in `daw.c` **after** `sample_chop.h` is included (via `soundsystem.h`), so the guard was always compiled out — dead code that looked alive.

This caused:
1. `renderPatternToBuffer` → `dawLoad` → re-bounce check sees live `chopState.sourcePath` → calls `chopStateBounce()` recursively
2. The inner (recursive) bounce ran correctly and produced the right output
3. But it left dirty `_chop_melodyVoice`/`_chop_melodyVoiceCount` arrays
4. The inner `chopStateBounce` called `dawAudioUngate()`, prematurely unblocking the audio thread
5. The outer bounce then ran with corrupted voice tracking → wrong output

The first bounce after launch appeared to work because the inner bounce's output was loaded into the sampler. The outer bounce's corrupted output was discarded. On second bounce, the stale arrays from the first outer bounce caused visible corruption.

## Fix

1. **`_chopBounceActive` flag** (`sample_chop.h`): Set true before `dawLoad`, checked by `daw_file.h`'s re-bounce guard. Replaces the broken `#ifdef DAW_HAS_CHOP_STATE` guard.

2. **Explicit context init** (`sample_chop.h`): Call `initSynthContext`/`initEffectsContext`/`initMixerContext` before syncing state, matching CLI's initialization order. Prevents `_ensure*Ctx()` from wiping synced params during the render loop.

## Test

`test_daw_file.c`: `verifyBounceGuardPreventsRebounce()` — saves a song with `[sample]` section, loads with `_chopBounceActive=true`, verifies `chopStateBounce` is NOT called. Then loads without the flag and verifies it IS called.

## Lesson

Don't use `#ifdef` guards across header boundaries — include order makes them fragile and produces silent dead code. Use runtime flags instead.
