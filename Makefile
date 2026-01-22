CC := clang
MACOSX_DEPLOYMENT_TARGET ?= 14.0
export MACOSX_DEPLOYMENT_TARGET

CFLAGS  := -std=c11 -O2 -g -I. -Wall -Wextra
LDFLAGS := $(shell pkg-config --libs raylib)

BINDIR := bin

# Define your targets here
TARGETS := steer crowd path

# Source files for each target
steer_SRC       := steering/demo.c steering/steering.c
crowd_SRC       := crowd-experiment/demo.c
path_SRC        := pathing/demo.c pathing/grid.c pathing/terrain.c pathing/pathfinding.c pathing/mover.c

# Test targets
test_pathing_SRC  := tests/test_pathfinding.c pathing/grid.c pathing/terrain.c pathing/pathfinding.c
test_mover_SRC    := tests/test_mover.c pathing/grid.c pathing/pathfinding.c pathing/mover.c pathing/terrain.c
test_steering_SRC := tests/test_steering.c steering/steering.c

all: $(BINDIR) $(addprefix $(BINDIR)/,$(TARGETS))

$(BINDIR):
	mkdir -p $(BINDIR)

# Pattern rule: build any target from its corresponding _SRC
$(BINDIR)/%:
	$(CC) $(CFLAGS) -o $@ $($*_SRC) $(LDFLAGS)

# Pathing test - links raylib for GetTime() etc used in pathfinding.c
test_pathing: $(BINDIR)
	$(CC) $(CFLAGS) -o $(BINDIR)/$@ $(test_pathing_SRC) $(LDFLAGS)
	./$(BINDIR)/test_pathing

# Mover test
test_mover: $(BINDIR)
	$(CC) $(CFLAGS) -o $(BINDIR)/$@ $(test_mover_SRC) $(LDFLAGS)
	./$(BINDIR)/test_mover

# Steering test
test_steering: $(BINDIR)
	$(CC) $(CFLAGS) -o $(BINDIR)/$@ $(test_steering_SRC) $(LDFLAGS)
	./$(BINDIR)/test_steering

# Run all tests
test: test_pathing test_mover test_steering

# Aliases for convenience (make path, make steer, make crowd)
path: $(BINDIR) $(BINDIR)/path
steer: $(BINDIR) $(BINDIR)/steer
crowd: $(BINDIR) $(BINDIR)/crowd

# Texture atlas generator
atlas_gen_SRC := tools/atlas_gen.c
atlas: $(BINDIR)
	$(CC) $(CFLAGS) -o $(BINDIR)/atlas_gen $(atlas_gen_SRC) $(LDFLAGS)
	./$(BINDIR)/atlas_gen

clean:
	rm -rf $(BINDIR)

# Debug build - no optimization, all sanitizers
debug: CFLAGS := -std=c11 -O0 -g -fsanitize=address,undefined -fno-omit-frame-pointer -I. -Wall -Wextra
debug: LDFLAGS += -fsanitize=address,undefined
debug: $(BINDIR)
	$(CC) $(CFLAGS) -o $(BINDIR)/path_debug $(path_SRC) $(LDFLAGS)

# AddressSanitizer build for memory debugging
asan: CFLAGS := -std=c11 -O1 -g -fsanitize=address -fno-omit-frame-pointer -I. -Wall -Wextra
asan: LDFLAGS += -fsanitize=address
asan: $(BINDIR)
	$(CC) $(CFLAGS) -o $(BINDIR)/path_asan $(path_SRC) $(LDFLAGS)

.PHONY: all clean test test_pathing test_mover test_steering path steer crowd asan debug atlas
