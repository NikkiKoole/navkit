# Feature Development Process

How we build new features using the `docs/doing/` workflow.

---

## Overview

Every new feature starts in `docs/doing/`. This folder is a focused workspace for the active feature - one feature at a time. When complete, we move it to `docs/done/` and clear the folder for the next feature.

---

## The Process

### 1. Define the Vertical Slice

Create `docs/doing/vertical-slice.md`:
- Pick the smallest testable chunk of the feature
- Write a concrete test scenario you can run manually
- Define clear success criteria ("I can do X, then Y happens")

Example from crafting:
```
Test: Mine wall -> orange item appears -> hauler stocks it -> 
      crafter fetches -> crafts stone blocks -> hauler stocks output
```

The vertical slice should be:
- **End-to-end**: Exercises the full loop, not isolated parts
- **Small**: Can be built in a focused session
- **Visible**: You can see it working in the game

### 2. Write the Technical Plan

Create `docs/doing/crafting-plan.md` (or `<feature>-plan.md`):
- Data structures needed
- New job types / state machines
- Integration points with existing systems
- File-by-file breakdown of changes

Don't over-plan. Just enough to start coding confidently.

### 3. Create the README Dashboard

Create `docs/doing/README.md`:
- Start with `## Status: IN PROGRESS`
- List what's done vs what's left
- Update this as you work

### 4. (Optional) Roadmap Doc

If the feature has future phases beyond the vertical slice, create a roadmap doc (e.g., `materials-and-workshops.md`). This captures ideas without cluttering the immediate plan.

### 5. Build It

Implement the vertical slice. Update README status as you go.

### 6. Mark Complete

When the vertical slice works:
- Update README to `## Status: DONE`
- Document what was actually built (may differ from plan)
- List key files for future reference

### 7. Move to Done

Move the folder contents:
```
docs/doing/* -> docs/done/<feature-name>/
```

Clear `docs/doing/` for the next feature.

---

## Folder Structure

```
docs/
├── doing/                # Active feature (one at a time)
│   ├── README.md         # Status dashboard
│   ├── vertical-slice.md # Test scenario / acceptance criteria  
│   ├── <feature>-plan.md # Technical spec
│   └── <roadmap>.md      # Future phases (optional)
│
├── done/                 # Completed features
│   ├── crafting-system/
│   ├── hauling/
│   └── ...
│
├── todo/                 # Actionable tasks and plans
│
└── vision/               # Future ideas (not yet planned)
```

---

## Why This Works

- **Focus**: One feature at a time in `doing/`
- **Clarity**: README shows exactly where you are
- **Scope control**: Vertical slice prevents over-building
- **History**: `done/` preserves decisions and context
- **Low overhead**: Just a few markdown files

---

## Quick Start Checklist

Starting a new feature:

- [ ] Clear or move anything in `docs/doing/` to `docs/done/`
- [ ] Create `vertical-slice.md` with test scenario
- [ ] Create `<feature>-plan.md` with technical approach
- [ ] Create `README.md` with status: IN PROGRESS
- [ ] Build the vertical slice
- [ ] Update README to DONE when complete
- [ ] Move to `docs/done/<feature-name>/`
