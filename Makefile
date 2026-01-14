CC := clang
MACOSX_DEPLOYMENT_TARGET ?= 14.0
export MACOSX_DEPLOYMENT_TARGET

CFLAGS  := -std=c11 -O2 -I. -Wall -Wextra
LDFLAGS := $(shell pkg-config --libs raylib)

# Define your targets here
TARGETS := crowd hpa-star

# Source files for each target
crowd_SRC     := crowd-steering/crowd-raylib-betterstats5.c
hpa-star_SRC  := hpa-star-tests/test7.c hpa-star-tests/grid.c hpa-star-tests/terrain.c hpa-star-tests/pathfinding.c

# Test target (no raylib needed for tests)
test_SRC      := tests/test_pathfinding.c hpa-star-tests/grid.c hpa-star-tests/terrain.c hpa-star-tests/pathfinding.c

all: $(TARGETS)

# Pattern rule: build any target from its corresponding _SRC
$(TARGETS):
	$(CC) $(CFLAGS) -o $@ $($@_SRC) $(LDFLAGS)

# Test target - links raylib for GetTime() etc used in pathfinding.c
test: $(test_SRC)
	$(CC) $(CFLAGS) -o $@ $(test_SRC) $(LDFLAGS)
	./test

clean:
	rm -f $(TARGETS) test

.PHONY: all clean run-crowd run-hpa $(TARGETS) test
