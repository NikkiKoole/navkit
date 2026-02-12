# Feature Completion Checklist

Before marking any job/feature as "done", verify:

## 1. Write End-to-End Tests

Create a test that runs the full flow:
- Minimal world setup
- Trigger job assignment
- Run simulation ticks
- Verify expected outcome

If the test doesn't pass, the feature isn't done.

## 2. Verify In-Game or Headless

```bash
./build/bin/path --headless --load save.bin.gz --ticks 500
```

Don't trust "it compiles" - actually see movers do the job.

## 3. Job System Wiring Checklist

For any new job type:
- [ ] Function declared in .h file
- [ ] Function implemented in .c file (not just declared!)
- [ ] Added to `jobDrivers[]` table
- [ ] Added to `AssignJobs()` with `hasXXXWork` flag
- [ ] WorkGiver checks the correct capability
- [ ] Capability field exists in `MoverCapabilities` struct
- [ ] Capability initialized in `InitMover()` and save/load
- [ ] Simulation tick function called in `TickWithDt()` if needed
- [ ] Inspector updated to show new capability/state

## 4. Use Inspector to Verify State

```bash
./build/bin/path --inspect save.bin.gz --designations    # See waiting work
./build/bin/path --inspect save.bin.gz --mover 0         # Check capabilities
./build/bin/path --inspect save.bin.gz --jobs-active     # See assigned jobs
```

## 5. UI for Testing

- Is there a way to trigger/test this feature from the UI?
- Are relevant parameters exposed in ui_panels.c for tuning?
- Does the inspector show the new state?

## 6. Reuse Existing Systems

Before writing new code, check if existing systems already handle it:
- Existing job patterns (haul, mine, build, chop)
- Existing designation patterns
- Existing item/stockpile patterns

Copy patterns from similar features rather than inventing new approaches.

## 7. Gate for Moving to `done/`

**Never move a doc to `done/` without explicit user permission.**

Before asking, verify:
- End-to-end test passes
- You've seen it work in-game or headless
- Inspector shows correct state

Then ask the user if it can be moved to `done/`.
