CC := clang
MACOSX_DEPLOYMENT_TARGET ?= 14.0
export MACOSX_DEPLOYMENT_TARGET

# Set to 1 to generate 16x16 atlas, 0 for 8x8 only
GENERATE_16X16 ?= 0

CFLAGS  := -std=c11 -O2 -g -I. -Ivendor -Wall -Wextra
LDFLAGS := $(shell pkg-config --libs raylib)

BINDIR := bin

# Define your targets here
TARGETS := steer crowd path soundsystem-demo

# Source files for each target (using unity build for path)
steer_SRC      := experiments/steering/demo.c experiments/steering/steering.c
crowd_SRC      := experiments/crowd/demo.c
path_SRC       := src/unity.c
soundsystem-demo_SRC := soundsystem/demo/demo.c

# Test targets - all use test_unity.c for shared game logic
test_pathing_SRC    := tests/test_pathfinding.c tests/test_unity.c
test_mover_SRC      := tests/test_mover.c tests/test_unity.c
test_steering_SRC   := tests/test_steering.c experiments/steering/steering.c
test_jobs_SRC       := tests/test_jobs.c tests/test_unity.c
test_water_SRC      := tests/test_water.c tests/test_unity.c
test_groundwear_SRC := tests/test_groundwear.c tests/test_unity.c
test_fire_SRC       := tests/test_fire.c tests/test_unity.c
test_temperature_SRC := tests/test_temperature.c tests/test_unity.c
test_steam_SRC       := tests/test_steam.c tests/test_unity.c
test_time_SRC        := tests/test_time.c tests/test_unity.c
test_time_specs_SRC  := tests/test_time_specs.c tests/test_unity.c
test_high_speed_SRC  := tests/test_high_speed.c tests/test_unity.c

# Soundsystem test - standalone, no test_unity.c needed
test_soundsystem_SRC := tests/test_soundsystem.c

all: $(BINDIR) $(addprefix $(BINDIR)/,$(TARGETS)) $(BINDIR)/path8

$(BINDIR):
	mkdir -p $(BINDIR)

# Pattern rule: build any target from its corresponding _SRC
$(BINDIR)/%:
	$(CC) $(CFLAGS) -o $@ $($*_SRC) $(LDFLAGS)

# Soundsystem demo needs -Wno-unused-function for header-only library functions
$(BINDIR)/soundsystem-demo: $(soundsystem-demo_SRC) | $(BINDIR)
	$(CC) $(CFLAGS) -Wno-unused-function -Wno-unused-variable -o $@ $(soundsystem-demo_SRC) $(LDFLAGS)

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

# Jobs test
test_jobs: $(BINDIR)
	$(CC) $(CFLAGS) -o $(BINDIR)/$@ $(test_jobs_SRC) $(LDFLAGS)
	./$(BINDIR)/test_jobs

# Water test
test_water: $(BINDIR)
	$(CC) $(CFLAGS) -o $(BINDIR)/$@ $(test_water_SRC) $(LDFLAGS)
	./$(BINDIR)/test_water

# Ground wear test
test_groundwear: $(BINDIR)
	$(CC) $(CFLAGS) -o $(BINDIR)/$@ $(test_groundwear_SRC) $(LDFLAGS)
	./$(BINDIR)/test_groundwear

# Fire test
test_fire: $(BINDIR)
	$(CC) $(CFLAGS) -o $(BINDIR)/$@ $(test_fire_SRC) $(LDFLAGS)
	./$(BINDIR)/test_fire

# Temperature test
test_temperature: $(BINDIR)
	$(CC) $(CFLAGS) -o $(BINDIR)/$@ $(test_temperature_SRC) $(LDFLAGS)
	./$(BINDIR)/test_temperature

# Steam test
test_steam: $(BINDIR)
	$(CC) $(CFLAGS) -o $(BINDIR)/$@ $(test_steam_SRC) $(LDFLAGS)
	./$(BINDIR)/test_steam

# Time test
test_time: $(BINDIR)
	$(CC) $(CFLAGS) -o $(BINDIR)/$@ $(test_time_SRC) $(LDFLAGS)
	./$(BINDIR)/test_time

# Time specification tests
test_time_specs: $(BINDIR)
	$(CC) $(CFLAGS) -o $(BINDIR)/$@ $(test_time_specs_SRC) $(LDFLAGS)
	./$(BINDIR)/test_time_specs

# High game speed safety tests
test_high_speed: $(BINDIR)
	$(CC) $(CFLAGS) -o $(BINDIR)/$@ $(test_high_speed_SRC) $(LDFLAGS)
	./$(BINDIR)/test_high_speed

# Soundsystem tests - standalone audio library tests
test_soundsystem: $(BINDIR)
	$(CC) $(CFLAGS) -o $(BINDIR)/$@ $(test_soundsystem_SRC) -lm
	./$(BINDIR)/test_soundsystem

# Run all tests in standard (DF-style) mode - this is the default
test: test_pathing test_mover test_steering test_jobs test_water test_groundwear test_fire test_temperature test_steam test_time test_time_specs test_high_speed test_soundsystem

# Run all tests in legacy mode
test-legacy: $(BINDIR)
	@echo "=== Running all tests in LEGACY mode ==="
	./$(BINDIR)/test_pathing --legacy
	./$(BINDIR)/test_mover --legacy
	./$(BINDIR)/test_steering
	./$(BINDIR)/test_jobs --legacy
	./$(BINDIR)/test_water --legacy
	./$(BINDIR)/test_groundwear --legacy
	./$(BINDIR)/test_fire --legacy
	./$(BINDIR)/test_temperature --legacy
	./$(BINDIR)/test_steam --legacy
	./$(BINDIR)/test_time --legacy
	./$(BINDIR)/test_time_specs --legacy
	./$(BINDIR)/test_high_speed --legacy

# Run all tests in both modes (standard first, then legacy)
test-both: test test-legacy
	@echo "=== All tests passed in both modes ==="

# Benchmark targets - all use test_unity.c for shared game logic
bench_jobs_SRC := tests/bench_jobs.c tests/test_unity.c

# Job system benchmark
bench_jobs: $(BINDIR)
	$(CC) $(CFLAGS) -o $(BINDIR)/$@ $(bench_jobs_SRC) $(LDFLAGS)
	./$(BINDIR)/bench_jobs

# Run all benchmarks
bench: bench_jobs

# Aliases for convenience (make path, make steer, make crowd, make soundsystem-demo)
path: $(BINDIR) $(BINDIR)/path
steer: $(BINDIR) $(BINDIR)/steer
crowd: $(BINDIR) $(BINDIR)/crowd
soundsystem-demo: $(BINDIR) $(BINDIR)/soundsystem-demo

# Texture atlas generator
atlas_gen_SRC := tools/atlas_gen.c
atlas: $(BINDIR)
	$(CC) $(CFLAGS) -DGENERATE_16X16=$(GENERATE_16X16) -o $(BINDIR)/atlas_gen $(atlas_gen_SRC) $(LDFLAGS)
	./$(BINDIR)/atlas_gen

# Font embedder
font_embed_SRC := tools/font_embed.c
embed_font: $(BINDIR)
	$(CC) $(CFLAGS) -o $(BINDIR)/font_embed $(font_embed_SRC)
	./$(BINDIR)/font_embed assets/fonts/comic.fnt assets/fonts/comic_embedded.h

# SCW (Single Cycle Waveform) embedder for soundsystem
scw_embed_SRC := soundsystem/tools/scw_embed.c
scw_embed: $(BINDIR)
	$(CC) $(CFLAGS) -o $(BINDIR)/scw_embed $(scw_embed_SRC)
	./$(BINDIR)/scw_embed soundsystem/cycles > soundsystem/engines/scw_data.h
	@echo "Generated soundsystem/engines/scw_data.h"

# Sample embedder for soundsystem (oneshot samples like CR-78)
sample_embed_SRC := soundsystem/tools/sample_embed.c
sample_embed: $(BINDIR)
	$(CC) $(CFLAGS) -o $(BINDIR)/sample_embed $(sample_embed_SRC)
	./$(BINDIR)/sample_embed soundsystem/oneshots > soundsystem/engines/sample_data.h
	@echo "Generated soundsystem/engines/sample_data.h"

# Embed all assets (atlas + fonts)
embed: atlas embed_font

# Build with 8x8 tiles
path8 $(BINDIR)/path8: $(BINDIR)
	$(CC) $(CFLAGS) -DTILE_SIZE=8 -o $(BINDIR)/path8 $(path_SRC) $(LDFLAGS)

# Build with 16x16 tiles
path16 $(BINDIR)/path16: $(BINDIR)
	$(CC) $(CFLAGS) -DTILE_SIZE=16 -o $(BINDIR)/path16 $(path_SRC) $(LDFLAGS)

# Build with soundsystem enabled
path-sound: $(BINDIR)
	$(CC) $(CFLAGS) -DUSE_SOUNDSYSTEM -o $(BINDIR)/path_sound $(path_SRC) $(LDFLAGS)

clean:
	rm -rf $(BINDIR)

clean-atlas:
	rm -f assets/atlas.h assets/atlas8x8.h assets/atlas8x8.png assets/atlas16x16.h assets/atlas16x16.png

# Fast build - no optimization, no sanitizers (~0.5s)
fast: $(BINDIR)
	$(CC) -std=c11 -O0 -g -I. -Wall -Wextra -o $(BINDIR)/path_fast $(path_SRC) $(LDFLAGS)
	ln -sf path_fast $(BINDIR)/path

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

# Release build - max optimization, no debug symbols
release: $(BINDIR)
	$(CC) -std=c11 -O3 -DNDEBUG -I. -Wall -Wextra -o $(BINDIR)/path_release $(path_SRC) $(LDFLAGS)

.PHONY: all clean clean-atlas test test-legacy test-both test_pathing test_mover test_steering test_jobs test_water test_groundwear test_fire test_temperature test_steam test_time test_time_specs test_high_speed test_soundsystem path steer crowd soundsystem-demo asan debug fast release atlas embed_font embed scw_embed sample_embed path8 path16 path-sound bench bench_jobs
