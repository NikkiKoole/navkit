# Tests and Benchmarks

## Walkability Modes

The game supports two walkability models:

- **Standard (DF-style)**: Cells are walkable if there's a solid block below them (walk ON blocks, not IN them). This is the default mode.
- **Legacy**: Cells have explicit walkability flags (`CELL_WALKABLE`, `CELL_FLOOR`, etc.). Used for backwards compatibility.

All tests run in **standard mode by default**. Use `--legacy` to run in legacy mode.

## Running Tests

### Run All Tests (Standard Mode)

```bash
make test              # Run all tests in standard mode
```

### Run All Tests in Legacy Mode

```bash
make test-legacy       # Run all tests in legacy mode
```

### Run All Tests in Both Modes

```bash
make test-both         # Run all tests in standard mode, then legacy mode
```

### Run a Specific Test Suite

```bash
make test_mover        # Run mover tests (standard mode)
make test_jobs         # Run job system tests
make test_pathing      # Run pathfinding tests (runs BOTH modes by default)
make test_water        # Run water simulation tests
make test_fire         # Run fire simulation tests
make test_groundwear   # Run ground wear tests
make test_steering     # Run steering tests (no walkability)
make test_temperature  # Run temperature simulation tests
make test_steam        # Run steam simulation tests
make test_time         # Run time system tests
make test_time_specs   # Run time specification tests
make test_high_speed   # Run high-speed simulation tests
```

### Run a Specific Test in Legacy Mode

```bash
./bin/test_mover --legacy
./bin/test_jobs --legacy
./bin/test_fire --legacy
# etc.
```

### Verbose Output

Add `-v` flag to see raylib log output:

```bash
./bin/test_mover -v
./bin/test_mover -v --legacy
```

## Special Cases

### test_pathfinding

The pathfinding tests (`make test_pathing`) run in **both modes by default** since pathfinding behavior differs significantly between walkability models. Use `--legacy` to run only in legacy mode:

```bash
make test_pathing              # Runs both standard AND legacy modes
./bin/test_pathing --legacy    # Runs only legacy mode
```

### test_steering

Steering tests don't use walkability at all (pure vector math), so they ignore the `--legacy` flag.

### bench_jobs

The job benchmark uses legacy mode internally (hardcoded) for consistent benchmark comparisons.

## Running Benchmarks

```bash
make bench             # Run all benchmarks
make bench_jobs        # Run job system benchmarks only
```

## File Structure

```
tests/
├── test_unity.c       # Shared game logic for all tests (includes src/*.c)
├── test_pathfinding.c # A*, JPS, HPA* pathfinding tests
├── test_mover.c       # Mover movement and pathfinding tests
├── test_jobs.c        # Job system and hauling tests
├── test_water.c       # Water simulation tests
├── test_fire.c        # Fire and smoke simulation tests
├── test_steam.c       # Steam simulation tests
├── test_temperature.c # Temperature simulation tests
├── test_groundwear.c  # Ground wear tests
├── test_steering.c    # Steering behavior tests
├── test_time.c        # Time system tests
├── test_time_specs.c  # Time specification tests
├── test_high_speed.c  # High-speed simulation safety tests
├── bench_jobs.c       # Job system benchmarks
└── README.md          # This file
```

## Writing Tests

Tests use the c89spec framework (`vendor/c89spec.h`):

```c
#include "../vendor/c89spec.h"
#include "../vendor/raylib.h"
#include "../src/world/grid.h"
// ... other headers

describe(feature_name) {
    it("should do something") {
        // Setup
        InitGrid();
        
        // Action
        DoSomething();
        
        // Assert
        expect(result == expected);
    }
}

int main(int argc, char* argv[]) {
    bool verbose = false;
    for (int i = 1; i < argc; i++) {
        if (argv[i][0] == '-' && argv[i][1] == 'v') verbose = true;
    }
    if (!verbose) SetTraceLogLevel(LOG_NONE);
    
    test(feature_name);
    return summary();
}
```

## Writing Benchmarks

```c
#include "../vendor/raylib.h"
#include "../src/world/grid.h"
#include <stdio.h>
#include <time.h>

static double GetBenchTime(void) {
    return (double)clock() / CLOCKS_PER_SEC;
}

static void BenchSomething(void) {
    printf("--- Something ---\n");
    
    int iterations = 1000;
    
    volatile int sum = 0;  // Prevent optimization
    double start = GetBenchTime();
    for (int i = 0; i < iterations; i++) {
        sum += DoSomething();
    }
    double elapsed = (GetBenchTime() - start) * 1000.0;
    (void)sum;
    
    printf("  %d iterations: %.3fms\n", iterations, elapsed);
}

int main(int argc, char* argv[]) {
    (void)argc; (void)argv;
    SetTraceLogLevel(LOG_NONE);
    
    printf("\n=== BENCHMARKS ===\n\n");
    BenchSomething();
    printf("Done.\n");
    return 0;
}
```

## Adding New Tests

1. Create `tests/test_<system>.c`
2. Include `test_unity.c` for game logic
3. Add to Makefile:
   ```makefile
   test_<system>_SRC := tests/test_<system>.c tests/test_unity.c
   
   test_<system>: $(BINDIR)
       $(CC) $(CFLAGS) -o $(BINDIR)/$@ $(test_<system>_SRC) $(LDFLAGS)
       ./$(BINDIR)/test_<system>
   ```
4. Add to the `test:` target
5. Add to `test-legacy:` target with `--legacy` flag
6. Add to `.PHONY` line
