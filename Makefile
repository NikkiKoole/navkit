CC := clang
MACOSX_DEPLOYMENT_TARGET ?= 14.0
export MACOSX_DEPLOYMENT_TARGET

CFLAGS  := -std=c11 -O2 -I. -Wall -Wextra
LDFLAGS := $(shell pkg-config --libs raylib)

# Define your targets here
TARGETS := steer path

# Source files for each target
steer_SRC       := steering/demo.c
path_SRC        := pathing/demo.c pathing/grid.c pathing/terrain.c pathing/pathfinding.c

# Test target (no raylib needed for tests)
test_SRC      := tests/test_pathfinding.c pathing/grid.c pathing/terrain.c pathing/pathfinding.c

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

.PHONY: all clean $(TARGETS) test
