# A work board for parallel sessions (the C way)

> Status: idea (2026-11-23). Nothing built; written down for later. The user does not want it
> built yet.

## The problem

Several Claude sessions work in this repo at once (often 4-5). Today they coordinate by
messaging each other ("I'll touch X, shout if you're in it"), by committing only their own hunks,
and by a hand-kept "Recently active" list at the top of `docs/todo/INDEX.md`. That works, but:

- nothing stops two sessions starting the same work; it relies on the heads-up messages
- a hand-kept recent list goes stale as soon as a session forgets to update it
- work that is ready to be done next has no single place where it waits

## The model to copy: fp.engine.new's board

The sibling repo `../fp.engine.new` has a file-backed kanban board (`board/SPEC.md` is the format,
`audits/BOARD.md` the argument for it, `tools/board*.mjs` the tools, ~2,900 lines of Node). What
makes it work is mostly **conventions**, not code:

- **One markdown file per card** (`board/cards/c-xxxxxx.md`, random id so two actors never collide)
  with frontmatter: `status` (`idea → todo → doing → review → done`, plus `blocked`), `kind`,
  `topics` (from a fixed list), `priority`, `owner`, `after: [c-…]` (comes after these cards).
  The body says what and why; a `## Log` at the end records who did what when.
- **`ready: true` marks a card as claimable.** In fp.engine.new only the human sets it.
  **For navkit: not human-only (user's decision, 2026-11-23)**: a session may mark a card ready
  too, e.g. when the card says what to do and nothing it waits on is open. The user can still
  set or clear it like anyone else.
- **Claiming is a committed edit**: `claimed_at` + `lease_expires` in the card. A rejected push
  means someone else claimed first (git is the compare-and-swap; no server). A crashed session's
  lease runs out.
- **`blocked` + one-sentence `question`** is the one defined way for an agent to ask the user.
- **Done cards are never moved or deleted**: finished work stays findable.
- Generated views: a README listing ready cards and `after:` lines, a browser view with columns.

For navkit the **"Recently active" list would become a view** (cards in `doing` / `review`, newest
`done`) instead of something every session has to remember to edit.

The ~100 design docs in `docs/todo/` would stay as they are: cards point at them (a `context:`
field), the way fp.engine.new's cards point at `audits/`.

## Three C-flavoured ways, lightest first

1. **No program: the format plus shell.** The frontmatter is simple enough for `grep` / `awk`:
   `grep -l "status: doing" board/cards/*`, ready + todo with two greps. A small `tools/board.sh`
   (like `tools/scenarios_check.sh`) for list, claim (edit the file, commit, push; a rejected push
   = someone was first) and a lint run from `make`. Cheapest, and it fits how navkit's tooling
   already works. Weak spot: awk gets ugly for `after:` lines and lease expiry.

2. **A small separate C program**: `tools/board.c`, built by `make board`, its own `main`, not in
   the game's unity build (like the `nav/` standalone checks). Parsing this frontmatter is roughly
   150-200 lines of C; list, claim, `after:` ordering and leases (`time()`) are straightforward;
   git stays a `system("git ...")` call. No browser view, but it can write a static
   `board/README.md` or an HTML page. Two to three times the code of the JS for the same features
   (C strings).

3. **A raylib board view**: columns of cards drawn in a window with the game's UI code; the one
   thing that would be nicer in C than the web view. As a separate small raylib program, not
   inside `path`. A toy for later.

## Recommendation (when it is picked up)

Start with **option 1**: the format and the habits (claim before starting, mark a card ready
when it is complete enough to start, close with a log line) carry most of the value, and shell keeps it tiny. If listing and
ordering start to hurt, move to **option 2** on the same files, so nothing migrates. Option 3 only
if the user wants to look at the board as columns.

First cards would be what is active now (the "Recently active" lines in `INDEX.md` and their known
next steps), not a backfill of the old lists.

**The real cost is discipline, not language**: claims only help if every session claims before it
starts.

**Decided (2026-11-23): `ready` is not human-only here.** Sessions pick up work themselves today
and should keep doing so; `ready` just says "this card is complete enough to claim". So the
claim + lease is what prevents double work, not the gate.
