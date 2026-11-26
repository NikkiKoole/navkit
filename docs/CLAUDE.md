# Docs Workflow

## Structure

- `docs/todo/` — All planned work. Each doc has a status line (`spec`, `partial`, `idea`, etc.)
- `docs/NOW.md` — **Read first.** What the project is today, the lenses (games) and their
  state, the next focus, standing decisions. Short; rewrite it, don't append
- `docs/todo/INDEX.md` — **Then here.** "Recently active" at the top (what is being worked on
  now, newest first), then every todo doc grouped by status, then `docs/research/`
- `docs/todo/architecture/` — Optimization research, structural plans (reference, not active tasks unless explicitly picked)
- `docs/done/` — Completed features
- `docs/bugs/` — Known bugs
- `docs/vision/` — Future ideas
- `docs/research/` — Measured findings and broad research (vehicles, metro, crowds, nav fuzzing); listed in INDEX.md
- `docs/reference/` — Older analyses (Feb 2026)
- `docs/doing/` — Music/DAW (PixelSynth) work in progress, not the colony game
- **Historical, don't plan from these:** `MASTER-TODO.md`, `BACKLOG.md`, `PROJECT-ARC.md`,
  `blind-spots-and-fresh-todos.md` (Feb-Mar 2026), `todo/00-priority-order.md`. INDEX.md supersedes them.

## Guidelines

- Check `todo/INDEX.md` first when picking what to work on
- **When you finish something, add or update its line under "Recently active" in INDEX.md**
  (date, one line, the doc it lives in or the commits if it has no doc). Several sessions work
  in parallel; this list is how the next one finds what is current
- Recent history beyond that: `git log --since=YYYY-MM-DD --format="%ad %s" --date=short`
- Each doc's status line is the source of truth for its state
- Never move a doc to `done/` without user permission
