# Docs Workflow

How to use the docs folder structure.

## Folder Structure

```
docs/
├── todo/     # Actionable tasks and plans (pick from here)
├── doing/    # Active work (one thing at a time)
├── done/     # Completed features (archive)
├── vision/   # Future ideas, not yet planned
```

## Workflow

1. **Check `doing/`** - If something is there, continue working on it
2. **If `doing/` is empty** - Pick something from `todo/`
3. **Move the item to `doing/`** - This is now the active work
4. **Work on it** - Update the doc as you go
5. **When done** - Move the file to `done/`, update any cross-references

## Guidelines

- Only one thing in `doing/` at a time (focus)
- Update docs as you implement (keeps them accurate)
- When moving to `done/`, mark status as complete in the doc
- If a todo spawns new todos, add them to `todo/`
- Vision items become todos when they get concrete enough to implement

## Key Files

- `todo/roadmap.md` - Prioritized overview of what's next
- `todo/jobs-checklist.md` - Remaining jobs system work
- `todo/bugs.md` - Known bugs to fix
- `done/overview.md` - Project overview and architecture summary
