CC := clang
MACOSX_DEPLOYMENT_TARGET ?= 14.0
export MACOSX_DEPLOYMENT_TARGET

# Set to 1 to generate 16x16 atlas, 0 for 8x8 only
GENERATE_16X16 ?= 0

CFLAGS  := -std=c11 -O2 -g -I. -Ivendor -Wall -Wextra

BINDIR := bin

# ---------------------------------------------------------------------------
# Vendored raylib (built from vendor/raylib/)
# ---------------------------------------------------------------------------
RAYLIB_DIR    := vendor/raylib
RAYLIB_LIB    := $(BINDIR)/libraylib.a
RAYLIB_CFLAGS := -std=c11 -O2 -I$(RAYLIB_DIR) -I$(RAYLIB_DIR)/external/glfw/include \
                 -DPLATFORM_DESKTOP -DGRAPHICS_API_OPENGL_33 \
                 -Wno-unused-function -Wno-unused-variable -Wno-unused-parameter \
                 -Wno-missing-field-initializers -Wno-implicit-fallthrough
RAYLIB_SRCS   := $(RAYLIB_DIR)/rcore.c $(RAYLIB_DIR)/rshapes.c $(RAYLIB_DIR)/rtextures.c \
                 $(RAYLIB_DIR)/rtext.c $(RAYLIB_DIR)/rmodels.c $(RAYLIB_DIR)/raudio.c \
                 $(RAYLIB_DIR)/rglfw.c
RAYLIB_OBJS   := $(patsubst $(RAYLIB_DIR)/%.c,$(BINDIR)/raylib_%.o,$(RAYLIB_SRCS))

# macOS link flags for raylib (static)
LDFLAGS := -L$(BINDIR) -lraylib -framework OpenGL -framework Cocoa -framework IOKit \
           -framework CoreAudio -framework CoreVideo -lm -lpthread

# Build individual raylib object files
$(BINDIR)/raylib_%.o: $(RAYLIB_DIR)/%.c | $(BINDIR)
	@$(CC) $(RAYLIB_CFLAGS) -c -o $@ $<

# rglfw includes Objective-C (.m) files on macOS
$(BINDIR)/raylib_rglfw.o: $(RAYLIB_DIR)/rglfw.c | $(BINDIR)
	@$(CC) $(RAYLIB_CFLAGS) -x objective-c -c -o $@ $<

# Archive into static library
$(RAYLIB_LIB): $(RAYLIB_OBJS)
	@ar rcs $@ $^
	@echo "Built $(RAYLIB_LIB)"

# Define your targets here
TARGETS := steer crowd path soundsystem-demo

# Source files for each target (using unity build for path)
steer_SRC      := experiments/steering/demo.c experiments/steering/steering.c
crowd_SRC      := experiments/crowd/demo.c
path_SRC       := src/unity.c src/sound/sound_phrase.c src/sound/sound_synth_bridge.c
soundsystem-demo_SRC := soundsystem/demo/demo.c
sound_phrase_wav_SRC := tools/sound_phrase_wav.c src/sound/sound_phrase.c

# Test targets - all link against precompiled test_unity.o for shared game logic
TEST_UNITY_OBJ := $(BINDIR)/test_unity.o
test_pathing_SRC    := tests/test_pathfinding.c
test_mover_SRC      := tests/test_mover.c
test_steering_SRC   := tests/test_steering.c experiments/steering/steering.c
test_jobs_SRC       := tests/test_jobs.c
test_water_SRC      := tests/test_water.c
test_groundwear_SRC := tests/test_groundwear.c
test_fire_SRC       := tests/test_fire.c
test_temperature_SRC := tests/test_temperature.c
test_steam_SRC       := tests/test_steam.c
test_materials_SRC   := tests/test_materials.c
test_time_SRC        := tests/test_time.c
test_time_specs_SRC  := tests/test_time_specs.c
test_high_speed_SRC  := tests/test_high_speed.c
test_trees_SRC       := tests/test_trees.c
test_terrain_SRC     := tests/test_terrain.c
test_grid_audit_SRC  := tests/test_grid_audit.c
test_floordirt_SRC   := tests/test_floordirt.c
test_lighting_SRC    := tests/test_lighting.c

# Precompile test_unity.o once (the expensive part)
$(TEST_UNITY_OBJ): tests/test_unity.c $(RAYLIB_LIB) | $(BINDIR)
	$(CC) $(CFLAGS) -c -o $@ $<

# Soundsystem test - standalone, no test_unity.c needed
test_soundsystem_SRC := tests/test_soundsystem.c

all: $(RAYLIB_LIB) $(addprefix $(BINDIR)/,$(TARGETS)) $(BINDIR)/path8

$(BINDIR):
	mkdir -p $(BINDIR)

# Pattern rule: build any target from its corresponding _SRC
$(BINDIR)/%: $(RAYLIB_LIB)
	$(CC) $(CFLAGS) -o $@ $($*_SRC) $(LDFLAGS)

# Soundsystem demo needs -Wno-unused-function for header-only library functions
$(BINDIR)/soundsystem-demo: $(soundsystem-demo_SRC) | $(BINDIR)
	$(CC) $(CFLAGS) -Wno-unused-function -Wno-unused-variable -o $@ $(soundsystem-demo_SRC) $(LDFLAGS)

# Pathing test - links raylib for GetTime() etc used in pathfinding.c
test_pathing: $(TEST_UNITY_OBJ)
	@echo "Running pathfinding tests..."
	@$(CC) $(CFLAGS) -o $(BINDIR)/$@ $(test_pathing_SRC) $(TEST_UNITY_OBJ) $(LDFLAGS)
	-@./$(BINDIR)/test_pathing -q

# Mover test
test_mover: $(TEST_UNITY_OBJ)
	@echo "Running mover tests..."
	@$(CC) $(CFLAGS) -o $(BINDIR)/$@ $(test_mover_SRC) $(TEST_UNITY_OBJ) $(LDFLAGS)
	-@./$(BINDIR)/test_mover -q

# Steering test (doesn't use test_unity)
test_steering: $(BINDIR)
	@echo "Running steering tests..."
	@$(CC) $(CFLAGS) -o $(BINDIR)/$@ $(test_steering_SRC) $(LDFLAGS)
	-@./$(BINDIR)/test_steering -q

# Jobs test
test_jobs: $(TEST_UNITY_OBJ)
	@echo "Running jobs tests..."
	@$(CC) $(CFLAGS) -o $(BINDIR)/$@ $(test_jobs_SRC) $(TEST_UNITY_OBJ) $(LDFLAGS)
	-@./$(BINDIR)/test_jobs -q

# Water test
test_water: $(TEST_UNITY_OBJ)
	@echo "Running water tests..."
	@$(CC) $(CFLAGS) -o $(BINDIR)/$@ $(test_water_SRC) $(TEST_UNITY_OBJ) $(LDFLAGS)
	-@./$(BINDIR)/test_water -q

# Ground wear test
test_groundwear: $(TEST_UNITY_OBJ)
	@echo "Running groundwear tests..."
	@$(CC) $(CFLAGS) -o $(BINDIR)/$@ $(test_groundwear_SRC) $(TEST_UNITY_OBJ) $(LDFLAGS)
	-@./$(BINDIR)/test_groundwear -q

# Fire test
test_fire: $(TEST_UNITY_OBJ)
	@echo "Running fire tests..."
	@$(CC) $(CFLAGS) -o $(BINDIR)/$@ $(test_fire_SRC) $(TEST_UNITY_OBJ) $(LDFLAGS)
	-@./$(BINDIR)/test_fire -q

# Temperature test
test_temperature: $(TEST_UNITY_OBJ)
	@echo "Running temperature tests..."
	@$(CC) $(CFLAGS) -o $(BINDIR)/$@ $(test_temperature_SRC) $(TEST_UNITY_OBJ) $(LDFLAGS)
	-@./$(BINDIR)/test_temperature -q

# Steam test
test_steam: $(TEST_UNITY_OBJ)
	@echo "Running steam tests..."
	@$(CC) $(CFLAGS) -o $(BINDIR)/$@ $(test_steam_SRC) $(TEST_UNITY_OBJ) $(LDFLAGS)
	-@./$(BINDIR)/test_steam -q

# Materials test
test_materials: $(TEST_UNITY_OBJ)
	@echo "Running materials tests..."
	@$(CC) $(CFLAGS) -o $(BINDIR)/$@ $(test_materials_SRC) $(TEST_UNITY_OBJ) $(LDFLAGS)
	-@./$(BINDIR)/test_materials -q

# Time test
test_time: $(TEST_UNITY_OBJ)
	@echo "Running time tests..."
	@$(CC) $(CFLAGS) -o $(BINDIR)/$@ $(test_time_SRC) $(TEST_UNITY_OBJ) $(LDFLAGS)
	-@./$(BINDIR)/test_time -q

# Time specification tests
test_time_specs: $(TEST_UNITY_OBJ)
	@echo "Running time specs tests..."
	@$(CC) $(CFLAGS) -o $(BINDIR)/$@ $(test_time_specs_SRC) $(TEST_UNITY_OBJ) $(LDFLAGS)
	-@./$(BINDIR)/test_time_specs -q

# High game speed safety tests
test_high_speed: $(TEST_UNITY_OBJ)
	@echo "Running high speed tests..."
	@$(CC) $(CFLAGS) -o $(BINDIR)/$@ $(test_high_speed_SRC) $(TEST_UNITY_OBJ) $(LDFLAGS)
	-@./$(BINDIR)/test_high_speed -q

# Trees test
test_trees: $(TEST_UNITY_OBJ)
	@echo "Running trees tests..."
	@$(CC) $(CFLAGS) -o $(BINDIR)/$@ $(test_trees_SRC) $(TEST_UNITY_OBJ) $(LDFLAGS)
	-@./$(BINDIR)/test_trees -q

# Terrain test
test_terrain: $(TEST_UNITY_OBJ)
	@echo "Running terrain tests..."
	@$(CC) $(CFLAGS) -o $(BINDIR)/$@ $(test_terrain_SRC) $(TEST_UNITY_OBJ) $(LDFLAGS)
	-@./$(BINDIR)/test_terrain -q

# Grid audit test (grid.md findings)
test_grid_audit: $(TEST_UNITY_OBJ)
	@echo "Running grid audit tests..."
	@$(CC) $(CFLAGS) -o $(BINDIR)/$@ $(test_grid_audit_SRC) $(TEST_UNITY_OBJ) $(LDFLAGS)
	-@./$(BINDIR)/test_grid_audit -q

# Floor dirt test
test_floordirt: $(TEST_UNITY_OBJ)
	@echo "Running floordirt tests..."
	@$(CC) $(CFLAGS) -o $(BINDIR)/$@ $(test_floordirt_SRC) $(TEST_UNITY_OBJ) $(LDFLAGS)
	-@./$(BINDIR)/test_floordirt -q

# Lighting test
test_lighting: $(TEST_UNITY_OBJ)
	@echo "Running lighting tests..."
	@$(CC) $(CFLAGS) -o $(BINDIR)/$@ $(test_lighting_SRC) $(TEST_UNITY_OBJ) $(LDFLAGS)
	-@./$(BINDIR)/test_lighting -q

# Soundsystem tests - standalone audio library tests
test_soundsystem: $(BINDIR)
	@echo "Running soundsystem tests..."
	@$(CC) $(CFLAGS) -o $(BINDIR)/$@ $(test_soundsystem_SRC) -lm
	-@./$(BINDIR)/test_soundsystem -q

# Run all tests (mover uses 5 stress iterations by default)
.IGNORE: test
test: test_pathing test_mover test_steering test_jobs test_water test_groundwear test_fire test_temperature test_steam test_materials test_time test_time_specs test_high_speed test_trees test_terrain test_grid_audit test_floordirt test_lighting test_soundsystem

# Full stress tests - mover tests use 20 iterations
test-full: $(TEST_UNITY_OBJ)
	$(CC) $(CFLAGS) -DSTRESS_TEST_ITERATIONS=20 -o $(BINDIR)/test_mover $(test_mover_SRC) $(TEST_UNITY_OBJ) $(LDFLAGS)
	./$(BINDIR)/test_mover
	$(MAKE) test_pathing test_steering test_jobs test_water test_groundwear test_fire test_temperature test_steam test_time test_time_specs test_high_speed test_soundsystem

# Quick tests - skips mover tests entirely (~4s)
test-quick: test_pathing test_steering test_jobs test_water test_groundwear test_fire test_temperature test_steam test_materials test_time test_time_specs test_high_speed test_trees test_terrain test_grid_audit test_floordirt test_lighting test_soundsystem

# Benchmark targets - link against precompiled test_unity.o
bench_jobs_SRC := tests/bench_jobs.c

# Job system benchmark
bench_jobs: $(TEST_UNITY_OBJ)
	$(CC) $(CFLAGS) -o $(BINDIR)/$@ $(bench_jobs_SRC) $(TEST_UNITY_OBJ) $(LDFLAGS)
	./$(BINDIR)/bench_jobs

# Run all benchmarks
bench: bench_jobs

# Aliases for convenience (make path, make steer, make crowd, make soundsystem-demo)
path: $(BINDIR) $(BINDIR)/path
steer: $(BINDIR) $(BINDIR)/steer
crowd: $(BINDIR) $(BINDIR)/crowd
soundsystem-demo: $(BINDIR) $(BINDIR)/soundsystem-demo
sound-phrase-wav: $(BINDIR)
	$(CC) $(CFLAGS) -o $(BINDIR)/sound_phrase_wav $(sound_phrase_wav_SRC) -lm

# Aseprite CLI
ASEPRITE := /Applications/Aseprite/aseprite

# Export slices from Aseprite to individual PNGs
slices:
	rm -f assets/textures8x8/*.png
	$(ASEPRITE) -b assets/worksheet8x8.ase --save-as "assets/textures8x8/{slice}.png"

# Texture atlas generator
atlas_gen_SRC := tools/atlas_gen.c
atlas: slices $(RAYLIB_LIB)
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
path8 $(BINDIR)/path8: $(RAYLIB_LIB)
	$(CC) $(CFLAGS) -DTILE_SIZE=8 -o $(BINDIR)/path8 $(path_SRC) $(LDFLAGS)

# Build with 16x16 tiles
path16 $(BINDIR)/path16: $(RAYLIB_LIB)
	$(CC) $(CFLAGS) -DTILE_SIZE=16 -o $(BINDIR)/path16 $(path_SRC) $(LDFLAGS)

# Build with soundsystem enabled
path-sound: $(RAYLIB_LIB)
	$(CC) $(CFLAGS) -DUSE_SOUNDSYSTEM -o $(BINDIR)/path_sound $(path_SRC) $(LDFLAGS)

# ---------------------------------------------------------------------------
# Windows cross-compile (requires: brew install mingw-w64)
# ---------------------------------------------------------------------------
WIN_CC      := x86_64-w64-mingw32-gcc
WIN_AR      := x86_64-w64-mingw32-ar
WIN_BINDIR  := bin/win64
WIN_RAYLIB  := $(WIN_BINDIR)/libraylib.a
WIN_CFLAGS  := -std=c11 -O2 -I. -Ivendor -Wall -Wextra
WIN_RCFLAGS := -std=c11 -O2 -I$(RAYLIB_DIR) -I$(RAYLIB_DIR)/external/glfw/include \
               -DPLATFORM_DESKTOP -DGRAPHICS_API_OPENGL_33 \
               -Wno-unused-function -Wno-unused-variable -Wno-unused-parameter \
               -Wno-missing-field-initializers -Wno-implicit-fallthrough
WIN_LDFLAGS := -L$(WIN_BINDIR) -lraylib -lopengl32 -lgdi32 -lwinmm -lm -lpthread -static -mwindows
WIN_RAYLIB_SRCS := $(RAYLIB_DIR)/rcore.c $(RAYLIB_DIR)/rshapes.c $(RAYLIB_DIR)/rtextures.c \
                   $(RAYLIB_DIR)/rtext.c $(RAYLIB_DIR)/rmodels.c $(RAYLIB_DIR)/raudio.c \
                   $(RAYLIB_DIR)/rglfw.c
WIN_RAYLIB_OBJS := $(patsubst $(RAYLIB_DIR)/%.c,$(WIN_BINDIR)/raylib_%.o,$(WIN_RAYLIB_SRCS))

$(WIN_BINDIR):
	mkdir -p $(WIN_BINDIR)

$(WIN_BINDIR)/raylib_%.o: $(RAYLIB_DIR)/%.c | $(WIN_BINDIR)
	@$(WIN_CC) $(WIN_RCFLAGS) -c -o $@ $<

$(WIN_RAYLIB): $(WIN_RAYLIB_OBJS)
	@$(WIN_AR) rcs $@ $^
	@echo "Built $(WIN_RAYLIB)"

windows: $(WIN_RAYLIB)
	$(WIN_CC) $(WIN_CFLAGS) -o $(WIN_BINDIR)/path.exe $(path_SRC) $(WIN_LDFLAGS)
	@echo "Built $(WIN_BINDIR)/path.exe"

clean:
	rm -rf $(BINDIR)

clean-atlas:
	rm -f assets/atlas.h assets/atlas8x8.h assets/atlas8x8.png assets/atlas16x16.h assets/atlas16x16.png

# Fast build - no optimization, no sanitizers (~0.5s)
fast: $(RAYLIB_LIB)
	$(CC) -std=c11 -O0  -fno-omit-frame-pointer  -g -I. -Wall -Wextra -o $(BINDIR)/path_fast $(path_SRC) $(LDFLAGS)
	ln -sf path_fast $(BINDIR)/path

# Debug build - no optimization, all sanitizers
debug: CFLAGS := -std=c11 -O0 -g -fsanitize=address,undefined -fno-omit-frame-pointer -I. -Wall -Wextra
debug: LDFLAGS += -fsanitize=address,undefined
debug: $(RAYLIB_LIB)
	$(CC) $(CFLAGS) -o $(BINDIR)/path_debug $(path_SRC) $(LDFLAGS)

# AddressSanitizer build for memory debugging
asan: CFLAGS := -std=c11 -O1 -g -fsanitize=address -fno-omit-frame-pointer -I. -Wall -Wextra
asan: LDFLAGS += -fsanitize=address
asan: $(RAYLIB_LIB)
	$(CC) $(CFLAGS) -o $(BINDIR)/path_asan $(path_SRC) $(LDFLAGS)

# Release build - max optimization, no debug symbols
release: $(RAYLIB_LIB)
	$(CC) -std=c11 -O3 -DNDEBUG -I. -Wall -Wextra -o $(BINDIR)/path_release $(path_SRC) $(LDFLAGS)

.PHONY: all clean clean-atlas test test-legacy test-both test_pathing test_mover test_steering test_jobs test_water test_groundwear test_fire test_temperature test_steam test_materials test_time test_time_specs test_high_speed test_soundsystem test_floordirt test_lighting path steer crowd soundsystem-demo sound-phrase-wav asan debug fast release slices atlas embed_font embed scw_embed sample_embed path8 path16 path-sound bench bench_jobs windows
