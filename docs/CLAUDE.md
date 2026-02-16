# Docs Workflow

## Folder Structure

```
docs/
├── todo/           # Actionable tasks (pick anything)
│   └── architecture/  # Optimization research, structural plans
├── doing/          # Active work (one thing at a time)
├── done/           # Completed features
├── bugs/           # Known bugs
├── vision/         # Future ideas
└── checklists/     # Completion checklists
```

## Workflow

1. **Check `doing/`** — if something is there, continue working on it
2. **If `doing/` is empty** — show summary of `todo/` items, let user pick
3. **Move picked item to `doing/`** — work on it, update doc as you go
4. **When done** — ask user permission, then move to `done/<category>/`

## Guidelines

- Only one thing in `doing/` at a time
- Never move to `done/` without user permission
- See `docs/checklists/feature-completion.md` before completing features
- Architecture docs in `todo/architecture/` are reference/research — not active tasks unless explicitly picked
