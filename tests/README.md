# Tests and Benchmarks

## Running Tests

```bash
make test              # Run all tests
make test_mover        # Run mover tests only
make test_jobs         # Run job system tests only
make test_pathfinding  # Run pathfinding tests only
make test_water        # Run water simulation tests only
make test_fire         # Run fire simulation tests only
make test_groundwear   # Run ground wear tests only
make test_steering     # Run steering tests only
```

### Verbose Output

Add `-v` flag to see raylib log output:
```bash
./bin/test_mover -v
```

## Running Benchmarks

```bash
make bench             # Run all benchmarks
make bench_jobs        # Run job system benchmarks only
```

### Adding New Benchmarks

1. Create `tests/bench_<system>.c`
2. Include `test_unity.c` for game logic
3. Add to Makefile:
   ```makefile
   bench_<system>_SRC := tests/bench_<system>.c tests/test_unity.c
   
   bench_<system>: $(BINDIR)
       $(CC) $(CFLAGS) -o $(BINDIR)/$@ $(bench_<system>_SRC) $(LDFLAGS)
       ./$(BINDIR)/bench_<system>
   
   bench: bench_jobs bench_<system>
   ```
4. Add to `.PHONY` line

## File Structure

```
tests/
├── test_unity.c       # Shared game logic for all tests (includes src/*.c)
├── test_mover.c       # Mover movement and pathfinding tests
├── test_jobs.c        # Job system and hauling tests
├── test_pathfinding.c # A*, JPS, HPA* pathfinding tests
├── test_water.c       # Water simulation tests
├── test_fire.c        # Fire and smoke simulation tests
├── test_groundwear.c  # Ground wear tests
├── test_steering.c    # Steering behavior tests
└── bench_jobs.c       # Job system benchmarks
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
    return 0;
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
