CC := clang
MACOSX_DEPLOYMENT_TARGET ?= 14.0
export MACOSX_DEPLOYMENT_TARGET

CFLAGS  := -std=c11 -O2 -I. -Wall -Wextra
LDFLAGS := $(shell pkg-config --libs raylib)

# Define your targets here
TARGETS := steer crowd path

# Source files for each target
steer_SRC       := steering/demo.c steering/steering.c
crowd_SRC       := crowd-experiment/demo.c
path_SRC        := pathing/demo.c pathing/grid.c pathing/terrain.c pathing/pathfinding.c pathing/mover.c

# Test targets
test_pathing_SRC  := tests/test_pathfinding.c pathing/grid.c pathing/terrain.c pathing/pathfinding.c
test_mover_SRC    := tests/test_mover.c pathing/grid.c pathing/pathfinding.c pathing/mover.c
test_steering_SRC := tests/test_steering.c steering/steering.c

all: $(TARGETS)

# Pattern rule: build any target from its corresponding _SRC
$(TARGETS):
	$(CC) $(CFLAGS) -o $@ $($@_SRC) $(LDFLAGS)

# Pathing test - links raylib for GetTime() etc used in pathfinding.c
test_pathing: $(test_pathing_SRC)
	$(CC) $(CFLAGS) -o $@ $(test_pathing_SRC) $(LDFLAGS)
	./test_pathing

# Mover test
test_mover: $(test_mover_SRC)
	$(CC) $(CFLAGS) -o $@ $(test_mover_SRC) $(LDFLAGS)
	./test_mover

# Steering test
test_steering: $(test_steering_SRC)
	$(CC) $(CFLAGS) -o $@ $(test_steering_SRC) $(LDFLAGS)
	./test_steering

# Run all tests
test: test_pathing test_mover test_steering

clean:
	rm -f $(TARGETS) test_pathing test_mover test_steering crowd

.PHONY: all clean $(TARGETS) test test_pathing test_mover test_steering
