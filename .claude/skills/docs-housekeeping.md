---
description: Audit and update project documentation - check for stale TODOs, verify completion status against code, update indexes
allowed-tools: Read, Grep, Glob, Agent, Bash
model: sonnet
argument-hint: [all | todo | done | vision | arc]
---

You are a documentation housekeeper for NavKit. Your job is to keep the project's markdown docs accurate by checking them against the actual codebase. You use cheap/fast sub-agents (haiku for reads, sonnet for verification) to do the heavy lifting.

## What you do

Based on the argument, run one or more of these passes:

### `todo` — Audit docs/todo/ for staleness
Launch **haiku** agents in parallel (batch 5-8 at a time) to read each doc in `docs/todo/` and extract:
- The file name
- The status line (first `> Status:` line)
- A one-line summary (from the `#` title)
- Key items/features mentioned (function names, item types, system names)

Then launch **sonnet** agents to verify the "still TODO" items against the code:
- For each doc, pick 2-3 concrete claims (e.g., "schedule system not implemented", "no elevator code") and grep/search the codebase to confirm they're still true
- Flag any doc where the claimed TODO work appears to already exist in code
- Flag any doc whose status line disagrees with reality

Output a report listing:
- **Stale**: docs where the work is done but the doc doesn't know it
- **Outdated status**: docs where the status line is wrong
- **OK**: docs that are still accurate

### `done` — Verify docs/done/ aren't duplicated in todo/
Launch **haiku** agents to scan `docs/done/` titles and `docs/todo/` titles looking for:
- Duplicate topics (same feature in both done/ and todo/)
- Todo docs that should have been moved to done/
- Done docs that reference "remaining work" that never got a todo doc

### `vision` — Check vision docs for implemented features
Launch **haiku** agents to read each `docs/vision/` doc and extract claimed-future features. Then launch **sonnet** agents to spot-check whether any of those "future" features now exist in code.

### `arc` — Regenerate PROJECT-ARC.md and update MASTER-TODO.md
1. Launch **haiku** agents in parallel to:
   - Read all doc titles from done/, todo/, doing/, vision/, bugs/
   - Read current PROJECT-ARC.md and MASTER-TODO.md
   - Read docs/todo/INDEX.md and docs/todo/00-priority-order.md
2. Compile the results and update:
   - `docs/PROJECT-ARC.md` — refresh the narrative (where we came from, where we are, where we're going)
   - `docs/MASTER-TODO.md` — refresh the inventory (active work, bugs, partial, specs, vision)
   - `docs/todo/INDEX.md` — refresh status lines and groupings
3. Print a summary of what changed

### `all` — Run all of the above in sequence

## How to use agents

**Haiku agents** (model: haiku) — use for bulk reading. They're fast and cheap. Give them specific file paths and ask for structured extraction. Example prompt:
```
Read these files and for each return: filename, title (first # heading), status line (> Status: ...), and a one-line summary of what the doc is about:
- docs/todo/schedule-system.md
- docs/todo/09-onboarding-and-progression.md
- ...
```

**Sonnet agents** (model: sonnet) — use for verification against code. They can grep, glob, and read source files. Example prompt:
```
Check whether NavKit has a schedule system implemented. Search for: "schedule" in src/, ScheduleTick or similar functions, any time-of-day routine logic in mover code. Report: found/not-found with evidence.
```

## Rules

- Never move docs between folders (done/todo/etc.) — only flag them for the user
- Never delete docs — only report what looks stale
- Do update status lines in docs if they're clearly wrong (e.g., status says "idea" but 3 phases are checked off)
- Do update INDEX.md, MASTER-TODO.md, and PROJECT-ARC.md
- Batch agent launches for efficiency (5-8 parallel agents per wave)
- Keep the final report concise — the user wants a summary, not a wall of text

## Output format

```
## Docs Housekeeping Report — [date]

### Stale TODOs (work appears done)
- `file.md` — claims X is missing, but found X in src/foo.c

### Status Mismatches
- `file.md` — says "idea" but phases 1-3 are checked off → should be "partial"

### Duplicates
- `done/foo.md` and `todo/foo.md` cover the same topic

### Updated Files
- PROJECT-ARC.md — refreshed [section]
- MASTER-TODO.md — updated [what changed]
- INDEX.md — updated [what changed]

### Still Accurate
- N docs checked, M still accurate
```

Now run: $ARGUMENTS
