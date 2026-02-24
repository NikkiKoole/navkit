CC := clang
MACOSX_DEPLOYMENT_TARGET ?= 14.0
export MACOSX_DEPLOYMENT_TARGET

# Set to 1 to generate 16x16 atlas, 0 for 8x8 only
GENERATE_16X16 ?= 0

CFLAGS  := -std=c11 -pedantic -O2 -g -I. -Ivendor -Wall -Wextra \
           -Wcast-qual -Wcast-align -Wwrite-strings -Wundef -Wformat=2 \
           -Wstrict-prototypes -Wmissing-prototypes -Wredundant-decls -Wnested-externs \
           -Wno-shadow

# Test files compile at -O0 for speed (game logic in test_unity.o stays at -O2)
TCFLAGS := -std=c11 -pedantic -O0 -g -I. -Ivendor -Wall -Wextra \
           -Wcast-qual -Wcast-align -Wwrite-strings -Wundef -Wformat=2 \
           -Wstrict-prototypes -Wmissing-prototypes -Wredundant-decls -Wnested-externs \
           -Wno-shadow

BINDIR := build/bin

# ---------------------------------------------------------------------------
# Vendored raylib (built from vendor/raylib/)
# ---------------------------------------------------------------------------
RAYLIB_DIR    := vendor/raylib
RAYLIB_BUILDDIR := build/raylib
RAYLIB_LIB    := $(RAYLIB_BUILDDIR)/libraylib.a
RAYLIB_CFLAGS := -std=c11 -O2 -I$(RAYLIB_DIR) -I$(RAYLIB_DIR)/external/glfw/include \
                 -DPLATFORM_DESKTOP -DGRAPHICS_API_OPENGL_33 \
                 -Wno-unused-function -Wno-unused-variable -Wno-unused-parameter \
                 -Wno-missing-field-initializers -Wno-implicit-fallthrough
RAYLIB_SRCS   := $(RAYLIB_DIR)/rcore.c $(RAYLIB_DIR)/rshapes.c $(RAYLIB_DIR)/rtextures.c \
                 $(RAYLIB_DIR)/rtext.c $(RAYLIB_DIR)/raudio.c $(RAYLIB_DIR)/utils.c $(RAYLIB_DIR)/rglfw.c
RAYLIB_OBJS   := $(patsubst $(RAYLIB_DIR)/%.c,$(RAYLIB_BUILDDIR)/raylib_%.o,$(RAYLIB_SRCS))

# macOS link flags for raylib (static)
LDFLAGS := -L$(RAYLIB_BUILDDIR) -lraylib -framework OpenGL -framework Cocoa -framework IOKit \
           -framework CoreAudio -framework CoreVideo -lm -lpthread

# Build individual raylib object files
$(RAYLIB_BUILDDIR)/raylib_%.o: $(RAYLIB_DIR)/%.c | $(RAYLIB_BUILDDIR)
	@$(CC) $(RAYLIB_CFLAGS) -c -o $@ $<

# rglfw includes Objective-C (.m) files on macOS
$(RAYLIB_BUILDDIR)/raylib_rglfw.o: $(RAYLIB_DIR)/rglfw.c | $(RAYLIB_BUILDDIR)
	@$(CC) $(RAYLIB_CFLAGS) -x objective-c -c -o $@ $<

# Archive into static library
$(RAYLIB_LIB): $(RAYLIB_OBJS)
	@ar rcs $@ $^
	@echo "Built $(RAYLIB_LIB)"

# Define your targets here
TARGETS := steer crowd path soundsystem-demo mechanisms

# Source files for each target (using unity build for path)
steer_SRC      := experiments/steering/demo.c experiments/steering/steering.c
crowd_SRC      := experiments/crowd/demo.c
path_SRC       := src/unity.c src/sound/sound_phrase.c src/sound/sound_synth_bridge.c
soundsystem-demo_SRC := soundsystem/demo/demo.c
mechanisms_SRC := experiments/mechanisms/demo.c
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
test_mud_SRC         := tests/test_mud.c
test_seasons_SRC     := tests/test_seasons.c
test_weather_SRC     := tests/test_weather.c
test_wind_SRC        := tests/test_wind.c
test_snow_SRC        := tests/test_snow.c
test_thunderstorm_SRC := tests/test_thunderstorm.c
test_lighting_SRC    := tests/test_lighting.c
test_workshop_linking_SRC := tests/test_workshop_stockpile_linking.c
test_hunger_SRC      := tests/test_hunger.c
test_stacking_SRC    := tests/test_stacking.c
test_containers_SRC  := tests/test_containers.c
test_sleep_SRC       := tests/test_sleep.c
test_furniture_SRC   := tests/test_furniture.c
test_balance_SRC     := tests/test_balance.c
test_cross_z_SRC     := tests/test_cross_z.c
test_workshop_deconstruction_SRC := tests/test_workshop_deconstruction.c
test_tool_quality_SRC := tests/test_tool_quality.c
test_doors_SRC       := tests/test_doors.c
test_butchering_SRC  := tests/test_butchering.c
test_hunting_SRC     := tests/test_hunting.c
test_spoilage_SRC    := tests/test_spoilage.c
test_fog_SRC         := tests/test_fog.c

# ---------------------------------------------------------------------------
# Unity build dependency tracking
# ---------------------------------------------------------------------------
# Both unity.c and test_unity.c #include all .c/.h files, but make doesn't
# know about those includes. Without this, changing a header (e.g. adding an
# enum value) won't trigger a rebuild, causing stale-object bugs.
GAME_SOURCES := $(wildcard src/*.h src/*.c src/**/*.h src/**/*.c tests/test_unity.c)

# Precompile test_unity.o once (the expensive part)
$(TEST_UNITY_OBJ): $(GAME_SOURCES) $(RAYLIB_LIB) | $(BINDIR)
	$(CC) $(CFLAGS) -c -o $@ tests/test_unity.c

# Soundsystem test - standalone, no test_unity.c needed
test_soundsystem_SRC := tests/test_soundsystem.c

# Mechanisms test - standalone, no test_unity.c needed
test_mechanisms_SRC := tests/test_mechanisms.c

all: $(RAYLIB_LIB) $(addprefix $(BINDIR)/,$(TARGETS)) $(BINDIR)/path8

$(BINDIR):
	mkdir -p $(BINDIR)

$(RAYLIB_BUILDDIR):
	mkdir -p $(RAYLIB_BUILDDIR)

# Unity build targets need to rebuild when any source file changes
$(BINDIR)/path: $(GAME_SOURCES)
$(BINDIR)/path8: $(GAME_SOURCES)

# Pattern rule: build executable targets from their corresponding _SRC
# (only matches direct children of BINDIR, not subdirectories or .o files)
$(BINDIR)/%: $(RAYLIB_LIB) | $(BINDIR)
	@if [ -n "$($*_SRC)" ]; then $(CC) $(CFLAGS) -o $@ $($*_SRC) $(LDFLAGS); fi

# Soundsystem demo needs -Wno-unused-function for header-only library functions
$(BINDIR)/soundsystem-demo: $(soundsystem-demo_SRC) | $(BINDIR)
	$(CC) $(CFLAGS) -Wno-unused-function -Wno-unused-variable -o $@ $(soundsystem-demo_SRC) $(LDFLAGS)

# Pathing test - links raylib for GetTime() etc used in pathfinding.c
test_pathing: $(TEST_UNITY_OBJ)
	@echo "Running pathfinding tests..."
	@$(CC) $(TCFLAGS) -Wno-overlength-strings -o $(BINDIR)/$@ $(test_pathing_SRC) $(TEST_UNITY_OBJ) $(LDFLAGS)
	-@./$(BINDIR)/test_pathing -q

# Mover test
test_mover: $(TEST_UNITY_OBJ)
	@echo "Running mover tests..."
	@$(CC) $(TCFLAGS) -o $(BINDIR)/$@ $(test_mover_SRC) $(TEST_UNITY_OBJ) $(LDFLAGS)
	-@./$(BINDIR)/test_mover -q

# Steering test (doesn't use test_unity)
test_steering: $(BINDIR)
	@echo "Running steering tests..."
	@$(CC) $(TCFLAGS) -o $(BINDIR)/$@ $(test_steering_SRC) $(LDFLAGS)
	-@./$(BINDIR)/test_steering -q

# Jobs test
test_jobs: $(TEST_UNITY_OBJ)
	@echo "Running jobs tests..."
	@$(CC) $(TCFLAGS) -o $(BINDIR)/$@ $(test_jobs_SRC) $(TEST_UNITY_OBJ) $(LDFLAGS)
	-@./$(BINDIR)/test_jobs -q

# Water test
test_water: $(TEST_UNITY_OBJ)
	@echo "Running water tests..."
	@$(CC) $(TCFLAGS) -o $(BINDIR)/$@ $(test_water_SRC) $(TEST_UNITY_OBJ) $(LDFLAGS)
	-@./$(BINDIR)/test_water -q

# Ground wear test
test_groundwear: $(TEST_UNITY_OBJ)
	@echo "Running groundwear tests..."
	@$(CC) $(TCFLAGS) -o $(BINDIR)/$@ $(test_groundwear_SRC) $(TEST_UNITY_OBJ) $(LDFLAGS)
	-@./$(BINDIR)/test_groundwear -q

# Fire test
test_fire: $(TEST_UNITY_OBJ)
	@echo "Running fire tests..."
	@$(CC) $(TCFLAGS) -o $(BINDIR)/$@ $(test_fire_SRC) $(TEST_UNITY_OBJ) $(LDFLAGS)
	-@./$(BINDIR)/test_fire -q

# Temperature test
test_temperature: $(TEST_UNITY_OBJ)
	@echo "Running temperature tests..."
	@$(CC) $(TCFLAGS) -o $(BINDIR)/$@ $(test_temperature_SRC) $(TEST_UNITY_OBJ) $(LDFLAGS)
	-@./$(BINDIR)/test_temperature -q

# Steam test
test_steam: $(TEST_UNITY_OBJ)
	@echo "Running steam tests..."
	@$(CC) $(TCFLAGS) -o $(BINDIR)/$@ $(test_steam_SRC) $(TEST_UNITY_OBJ) $(LDFLAGS)
	-@./$(BINDIR)/test_steam -q

# Materials test
test_materials: $(TEST_UNITY_OBJ)
	@echo "Running materials tests..."
	@$(CC) $(TCFLAGS) -o $(BINDIR)/$@ $(test_materials_SRC) $(TEST_UNITY_OBJ) $(LDFLAGS)
	-@./$(BINDIR)/test_materials -q

# Time test
test_time: $(TEST_UNITY_OBJ)
	@echo "Running time tests..."
	@$(CC) $(TCFLAGS) -o $(BINDIR)/$@ $(test_time_SRC) $(TEST_UNITY_OBJ) $(LDFLAGS)
	-@./$(BINDIR)/test_time -q

# Time specification tests
test_time_specs: $(TEST_UNITY_OBJ)
	@echo "Running time specs tests..."
	@$(CC) $(TCFLAGS) -o $(BINDIR)/$@ $(test_time_specs_SRC) $(TEST_UNITY_OBJ) $(LDFLAGS)
	-@./$(BINDIR)/test_time_specs -q

# High game speed safety tests
test_high_speed: $(TEST_UNITY_OBJ)
	@echo "Running high speed tests..."
	@$(CC) $(TCFLAGS) -o $(BINDIR)/$@ $(test_high_speed_SRC) $(TEST_UNITY_OBJ) $(LDFLAGS)
	-@./$(BINDIR)/test_high_speed -q

# Trees test
test_trees: $(TEST_UNITY_OBJ)
	@echo "Running trees tests..."
	@$(CC) $(TCFLAGS) -o $(BINDIR)/$@ $(test_trees_SRC) $(TEST_UNITY_OBJ) $(LDFLAGS)
	-@./$(BINDIR)/test_trees -q

# Terrain test
test_terrain: $(TEST_UNITY_OBJ)
	@echo "Running terrain tests..."
	@$(CC) $(TCFLAGS) -o $(BINDIR)/$@ $(test_terrain_SRC) $(TEST_UNITY_OBJ) $(LDFLAGS)
	-@./$(BINDIR)/test_terrain -q

# Grid audit test (grid.md findings)
test_grid_audit: $(TEST_UNITY_OBJ)
	@echo "Running grid audit tests..."
	@$(CC) $(TCFLAGS) -o $(BINDIR)/$@ $(test_grid_audit_SRC) $(TEST_UNITY_OBJ) $(LDFLAGS)
	-@./$(BINDIR)/test_grid_audit -q

# Floor dirt test
test_floordirt: $(TEST_UNITY_OBJ)
	@echo "Running floordirt tests..."
	@$(CC) $(TCFLAGS) -o $(BINDIR)/$@ $(test_floordirt_SRC) $(TEST_UNITY_OBJ) $(LDFLAGS)
	-@./$(BINDIR)/test_floordirt -q

# Mud test
test_mud: $(TEST_UNITY_OBJ)
	@echo "Running mud tests..."
	@$(CC) $(TCFLAGS) -o $(BINDIR)/$@ $(test_mud_SRC) $(TEST_UNITY_OBJ) $(LDFLAGS)
	-@./$(BINDIR)/test_mud -q

# Seasons test
test_seasons: $(TEST_UNITY_OBJ)
	@echo "Running season tests..."
	@$(CC) $(TCFLAGS) -o $(BINDIR)/$@ $(test_seasons_SRC) $(TEST_UNITY_OBJ) $(LDFLAGS)
	-@./$(BINDIR)/test_seasons -q

# Weather test
test_weather: $(TEST_UNITY_OBJ)
	@echo "Running weather tests..."
	@$(CC) $(TCFLAGS) -o $(BINDIR)/$@ $(test_weather_SRC) $(TEST_UNITY_OBJ) $(LDFLAGS)
	-@./$(BINDIR)/test_weather -q

# Wind test
test_wind: $(TEST_UNITY_OBJ)
	@echo "Running wind tests..."
	@$(CC) $(TCFLAGS) -o $(BINDIR)/$@ $(test_wind_SRC) $(TEST_UNITY_OBJ) $(LDFLAGS)
	-@./$(BINDIR)/test_wind -q

# Snow test
test_snow: $(TEST_UNITY_OBJ)
	@echo "Running snow tests..."
	@$(CC) $(TCFLAGS) -o $(BINDIR)/$@ $(test_snow_SRC) $(TEST_UNITY_OBJ) $(LDFLAGS)
	-@./$(BINDIR)/test_snow -q

# Thunderstorm test
test_thunderstorm: $(TEST_UNITY_OBJ)
	@echo "Running thunderstorm tests..."
	@$(CC) $(TCFLAGS) -o $(BINDIR)/$@ $(test_thunderstorm_SRC) $(TEST_UNITY_OBJ) $(LDFLAGS)
	-@./$(BINDIR)/test_thunderstorm -q

# Lighting test
test_lighting: $(TEST_UNITY_OBJ)
	@echo "Running lighting tests..."
	@$(CC) $(TCFLAGS) -o $(BINDIR)/$@ $(test_lighting_SRC) $(TEST_UNITY_OBJ) $(LDFLAGS)
	-@./$(BINDIR)/test_lighting -q

test_workshop_linking: $(TEST_UNITY_OBJ)
	@echo "Running workshop-stockpile linking tests..."
	@$(CC) $(TCFLAGS) -o $(BINDIR)/$@ $(test_workshop_linking_SRC) $(TEST_UNITY_OBJ) $(LDFLAGS)
	-@./$(BINDIR)/test_workshop_linking -q

# Hunger test
test_hunger: $(TEST_UNITY_OBJ)
	@echo "Running hunger tests..."
	@$(CC) $(TCFLAGS) -o $(BINDIR)/$@ $(test_hunger_SRC) $(TEST_UNITY_OBJ) $(LDFLAGS)
	-@./$(BINDIR)/test_hunger -q

test_stacking: $(TEST_UNITY_OBJ)
	@echo "Running stacking tests..."
	@$(CC) $(TCFLAGS) -o $(BINDIR)/$@ $(test_stacking_SRC) $(TEST_UNITY_OBJ) $(LDFLAGS)
	-@./$(BINDIR)/test_stacking -q

test_containers: $(TEST_UNITY_OBJ)
	@echo "Running container tests..."
	@$(CC) $(TCFLAGS) -o $(BINDIR)/$@ $(test_containers_SRC) $(TEST_UNITY_OBJ) $(LDFLAGS)
	-@./$(BINDIR)/test_containers -q

test_sleep: $(TEST_UNITY_OBJ)
	@echo "Running sleep tests..."
	@$(CC) $(TCFLAGS) -o $(BINDIR)/$@ $(test_sleep_SRC) $(TEST_UNITY_OBJ) $(LDFLAGS)
	-@./$(BINDIR)/test_sleep -q

test_furniture: $(TEST_UNITY_OBJ)
	@echo "Running furniture tests..."
	@$(CC) $(TCFLAGS) -o $(BINDIR)/$@ $(test_furniture_SRC) $(TEST_UNITY_OBJ) $(LDFLAGS)
	-@./$(BINDIR)/test_furniture -q

test_balance: $(TEST_UNITY_OBJ)
	@echo "Running balance tests..."
	@$(CC) $(TCFLAGS) -o $(BINDIR)/$@ $(test_balance_SRC) $(TEST_UNITY_OBJ) $(LDFLAGS)
	-@./$(BINDIR)/test_balance -q

test_cross_z: $(TEST_UNITY_OBJ)
	@echo "Running cross-z-level tests..."
	@$(CC) $(TCFLAGS) -o $(BINDIR)/$@ $(test_cross_z_SRC) $(TEST_UNITY_OBJ) $(LDFLAGS)
	-@./$(BINDIR)/test_cross_z -q

test_workshop_deconstruction: $(TEST_UNITY_OBJ)
	@echo "Running workshop deconstruction tests..."
	@$(CC) $(TCFLAGS) -o $(BINDIR)/$@ $(test_workshop_deconstruction_SRC) $(TEST_UNITY_OBJ) $(LDFLAGS)
	-@./$(BINDIR)/test_workshop_deconstruction -q

test_tool_quality: $(TEST_UNITY_OBJ)
	@echo "Running tool quality tests..."
	@$(CC) $(TCFLAGS) -o $(BINDIR)/$@ $(test_tool_quality_SRC) $(TEST_UNITY_OBJ) $(LDFLAGS)
	-@./$(BINDIR)/test_tool_quality -q

test_doors: $(TEST_UNITY_OBJ)
	@echo "Running door & shelter tests..."
	@$(CC) $(TCFLAGS) -o $(BINDIR)/$@ $(test_doors_SRC) $(TEST_UNITY_OBJ) $(LDFLAGS)
	-@./$(BINDIR)/test_doors -q

test_butchering: $(TEST_UNITY_OBJ)
	@echo "Running butchering tests..."
	@$(CC) $(TCFLAGS) -o $(BINDIR)/$@ $(test_butchering_SRC) $(TEST_UNITY_OBJ) $(LDFLAGS)
	-@./$(BINDIR)/test_butchering -q

test_hunting: $(TEST_UNITY_OBJ)
	@echo "Running hunting tests..."
	@$(CC) $(TCFLAGS) -o $(BINDIR)/$@ $(test_hunting_SRC) $(TEST_UNITY_OBJ) $(LDFLAGS)
	-@./$(BINDIR)/test_hunting -q

test_spoilage: $(TEST_UNITY_OBJ)
	@echo "Running spoilage tests..."
	@$(CC) $(TCFLAGS) -o $(BINDIR)/$@ $(test_spoilage_SRC) $(TEST_UNITY_OBJ) $(LDFLAGS)
	-@./$(BINDIR)/test_spoilage -q

test_fog: $(TEST_UNITY_OBJ)
	@echo "Running fog of war tests..."
	@$(CC) $(TCFLAGS) -o $(BINDIR)/$@ $(test_fog_SRC) $(TEST_UNITY_OBJ) $(LDFLAGS)
	-@./$(BINDIR)/test_fog -q

# Soundsystem tests - standalone audio library tests
test_soundsystem: $(BINDIR)
	@echo "Running soundsystem tests..."
	@$(CC) $(TCFLAGS) -o $(BINDIR)/$@ $(test_soundsystem_SRC) -lm
	-@./$(BINDIR)/test_soundsystem -q

# Mechanisms tests - standalone automation sandbox tests
test_mechanisms: $(BINDIR)
	@echo "Running mechanisms tests..."
	@$(CC) $(TCFLAGS) -o $(BINDIR)/$@ $(test_mechanisms_SRC) -lm
	-@./$(BINDIR)/test_mechanisms -q

# Run all tests (mover uses 5 stress iterations by default)
.IGNORE: test
test: test_pathing test_mover test_steering test_jobs test_water test_groundwear test_fire test_temperature test_steam test_materials test_time test_time_specs test_high_speed test_trees test_terrain test_grid_audit test_floordirt test_mud test_seasons test_weather test_wind test_snow test_thunderstorm test_lighting test_workshop_linking test_hunger test_stacking test_containers test_sleep test_furniture test_balance test_soundsystem test_cross_z test_workshop_deconstruction test_tool_quality test_doors test_butchering test_hunting test_spoilage test_fog

# Full stress tests - mover tests use 20 iterations
test-full: $(TEST_UNITY_OBJ)
	$(CC) $(TCFLAGS) -DSTRESS_TEST_ITERATIONS=20 -o $(BINDIR)/test_mover $(test_mover_SRC) $(TEST_UNITY_OBJ) $(LDFLAGS)
	./$(BINDIR)/test_mover
	$(MAKE) test_pathing test_steering test_jobs test_water test_groundwear test_fire test_temperature test_steam test_time test_time_specs test_high_speed test_soundsystem

# Quick tests - skips mover tests entirely (~4s)
test-quick: test_pathing test_steering test_jobs test_water test_groundwear test_fire test_temperature test_steam test_materials test_time test_time_specs test_high_speed test_trees test_terrain test_grid_audit test_floordirt test_lighting test_soundsystem

# Benchmark targets - link against precompiled test_unity.o
bench_jobs_SRC := tests/bench_jobs.c
bench_items_SRC := tests/bench_items.c
bench_rehaul_mini_SRC := tests/bench_rehaul_mini.c
bench_pathfinding_SRC := tests/bench_pathfinding.c

# Job system benchmark
bench_jobs: $(TEST_UNITY_OBJ)
	$(CC) $(CFLAGS) -o $(BINDIR)/$@ $(bench_jobs_SRC) $(TEST_UNITY_OBJ) $(LDFLAGS)
	./$(BINDIR)/bench_jobs

# Item system benchmark (pre-containers baseline)
bench_items: $(TEST_UNITY_OBJ)
	$(CC) $(CFLAGS) -o $(BINDIR)/$@ $(bench_items_SRC) $(TEST_UNITY_OBJ) $(LDFLAGS)
	./$(BINDIR)/bench_items

# Rehaul-only benchmark (fast, for filter change testing)
bench_rehaul_mini: $(TEST_UNITY_OBJ)
	$(CC) $(CFLAGS) -o $(BINDIR)/$@ $(bench_rehaul_mini_SRC) $(TEST_UNITY_OBJ) $(LDFLAGS)
	./$(BINDIR)/bench_rehaul_mini

# Pathfinding benchmark (A* and HPA* variable cost overhead)
bench_pathfinding: $(TEST_UNITY_OBJ)
	$(CC) $(CFLAGS) -o $(BINDIR)/$@ $(bench_pathfinding_SRC) $(TEST_UNITY_OBJ) $(LDFLAGS)
	./$(BINDIR)/bench_pathfinding

# Run all benchmarks
bench: bench_jobs bench_items bench_pathfinding

# Aliases for convenience (make path, make steer, make crowd, make soundsystem-demo)
path: $(BINDIR) $(BINDIR)/path
steer: $(BINDIR) $(BINDIR)/steer
crowd: $(BINDIR) $(BINDIR)/crowd
soundsystem-demo: $(BINDIR) $(BINDIR)/soundsystem-demo
mechanisms: $(BINDIR) $(BINDIR)/mechanisms
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
atlas: slices $(RAYLIB_LIB) | $(BINDIR)
	$(CC) $(CFLAGS) -DGENERATE_16X16=$(GENERATE_16X16) -o $(BINDIR)/atlas_gen $(atlas_gen_SRC) $(LDFLAGS)
	./$(BINDIR)/atlas_gen

# Font embedder
font_embed_SRC := tools/font_embed.c
embed_font: $(BINDIR)
	$(CC) $(CFLAGS) -o $(BINDIR)/font_embed $(font_embed_SRC)
	./$(BINDIR)/font_embed assets/fonts/comic.fnt assets/fonts/comic_embedded.h EMBEDDED_FONT LoadEmbeddedFont
	./$(BINDIR)/font_embed assets/fonts/times-roman-bold.fnt assets/fonts/times_roman_embedded.h EMBEDDED_TIMES_ROMAN LoadEmbeddedTimesRomanFont

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
WIN_BINDIR  := build/win64
WIN_RAYLIB  := $(WIN_BINDIR)/libraylib.a
WIN_CFLAGS  := -std=c11 -O2 -I. -Ivendor -Wall -Wextra
WIN_RCFLAGS := -std=c11 -O2 -I$(RAYLIB_DIR) -I$(RAYLIB_DIR)/external/glfw/include \
               -DPLATFORM_DESKTOP -DGRAPHICS_API_OPENGL_33 \
               -Wno-unused-function -Wno-unused-variable -Wno-unused-parameter \
               -Wno-missing-field-initializers -Wno-implicit-fallthrough
WIN_LDFLAGS := -L$(WIN_BINDIR) -lraylib -lopengl32 -lgdi32 -lwinmm -lm -lpthread -static -mconsole -Wl,--stack,8388608
WIN_RAYLIB_SRCS := $(RAYLIB_DIR)/rcore.c $(RAYLIB_DIR)/rshapes.c $(RAYLIB_DIR)/rtextures.c \
                   $(RAYLIB_DIR)/rtext.c $(RAYLIB_DIR)/raudio.c $(RAYLIB_DIR)/utils.c $(RAYLIB_DIR)/rglfw.c
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
	rm -rf $(BINDIR) $(WIN_BINDIR)

clean-raylib:
	rm -rf $(RAYLIB_BUILDDIR)

clean-atlas:
	rm -f assets/atlas.h assets/atlas8x8.h assets/atlas8x8.png assets/atlas16x16.h assets/atlas16x16.png

# Fast build - no optimization, no sanitizers (~0.5s)
fast: $(RAYLIB_LIB) | $(BINDIR)
	$(CC) -std=c11 -O0  -fno-omit-frame-pointer  -g -I. -Wall -Wextra -o $(BINDIR)/path_fast $(path_SRC) $(LDFLAGS)
	ln -sfn path_fast $(BINDIR)/path

# Debug build - no optimization, debug symbols
debug: $(RAYLIB_LIB) | $(BINDIR)
	$(CC) -std=c11 -O0 -g -fno-omit-frame-pointer -I. -Wall -Wextra -o $(BINDIR)/path_debug $(path_SRC) $(LDFLAGS)

# AddressSanitizer build for memory debugging
asan: CFLAGS := -std=c11 -O1 -g -fsanitize=address -fno-omit-frame-pointer -I. -Wall -Wextra
asan: LDFLAGS += -fsanitize=address
asan: $(RAYLIB_LIB) | $(BINDIR)
	$(CC) $(CFLAGS) -o $(BINDIR)/path_asan $(path_SRC) $(LDFLAGS)

# Release build - max optimization, no debug symbols
release: $(RAYLIB_LIB) | $(BINDIR)
	$(CC) -std=c11 -O3 -DNDEBUG -I. -Wall -Wextra -o $(BINDIR)/path_release $(path_SRC) $(LDFLAGS)

.PHONY: all clean clean-raylib clean-atlas test test-legacy test-both test_pathing test_mover test_steering test_jobs test_water test_groundwear test_fire test_temperature test_steam test_materials test_time test_time_specs test_high_speed test_soundsystem test_floordirt test_lighting test_weather test_wind test_hunger test_balance test_fog path steer crowd soundsystem-demo mechanisms sound-phrase-wav asan debug fast release slices atlas embed_font embed scw_embed sample_embed path8 path16 path-sound bench bench_jobs bench_items windows
