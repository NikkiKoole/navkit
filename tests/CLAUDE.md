# tests/

Unit tests and benchmarks using c89spec framework.

## Running Tests

```bash
make test              # all tests
make test_jobs         # single test
make test-quick        # skip slow mover stress tests
make bench             # benchmarks (jobs + items)
```

## Test Unity Build

`test_unity.c` is a separate unity build that includes all game logic (`src/**/*.c`) minus rendering and main. It compiles to `test_unity.o` which all test binaries link against.

The Makefile tracks all source files as dependencies — `test_unity.o` rebuilds automatically when any `.c` or `.h` file changes.

## Writing a New Test

1. Create `tests/test_foo.c`
2. Include the test framework and headers:
   ```c
   #include "c89spec.h"
   #include "../src/world/grid.h"
   // ... other headers as needed
   ```
3. Use c89spec pattern:
   ```c
   static bool test_verbose = false;

   describe(test_foo) {
       it("should do bar") {
           InitGrid(32, 32, 4);
           ClearMovers();
           // ... test logic ...
           expect(result == expected);
       }
   }

   int main(int argc, char *argv[]) {
       test_verbose = c89spec_parse_args(argc, argv);
       if (!test_verbose) SetTraceLogLevel(LOG_NONE);

       test(test_foo);
       return summary();
   }
   ```
4. Add to Makefile:
   ```makefile
   test_foo_SRC := tests/test_foo.c

   test_foo: $(TEST_UNITY_OBJ)
       @echo "Running foo tests..."
       @$(CC) $(CFLAGS) -o $(BINDIR)/$@ $(test_foo_SRC) $(TEST_UNITY_OBJ) $(LDFLAGS)
       -@./$(BINDIR)/test_foo -q
   ```
5. Add `test_foo` to the `test:` target list and `.PHONY` list
6. Add `test_foo.c` entry to `src/unity.c` is NOT needed — tests use `test_unity.c` instead

## Test Isolation

Always clean state at the start of each test:
- `InitGrid(w, h, d)` — allocates fresh grids
- `ClearMovers()`, `ClearItems()`, `ClearJobs()` — reset entity pools
- `ClearDesignations()`, `ClearBlueprints()` — reset world state

## Flags

- `-q` (quiet): suppress test output, only show failures + summary
- `-v` (verbose): show raylib logs and detailed output
- `--tap`: machine-readable TAP (Test Anything Protocol) output, no ANSI colors

All flags are handled by `c89spec_parse_args(argc, argv)` in each test's `main()`. To add new flags, update `c89spec_parse_args()` in `vendor/c89spec.h` — all test binaries get it automatically.

### TAP output

```bash
./build/bin/test_jobs --tap     # single test binary
make test-tap                   # all tests
```

Output format — easy to grep/parse:
```
TAP version 13
# module_name
ok 1 - module_name: "test description"
not ok 2 - module_name: "test description"
  ---
  failed: some_condition == expected_value
  ...
1..N
```

Find failures: `./build/bin/test_foo --tap | grep "not ok"`

## State Audit in Tests

The state audit (`core/state_audit.c`) can be used in tests to verify data consistency after complex operations. The audit checks item↔stockpile, reservation↔job, and mover↔job invariants. See `tests/test_jobs.c` for examples of tests that exercise blueprint delivery with stacked items (a bug caught by the audit system).

## Gotchas

- **Test state leaks**: if a test doesn't call `ClearMovers()` etc., leftover state from previous tests causes flaky failures
- **Grid size**: tests typically use small grids (32x32x4) — don't assume 512x512x16
- **Cutscene/UI code** (`src/ui/cutscene.c`) is NOT included in `test_unity.c` — it's render-only with no testable game logic
