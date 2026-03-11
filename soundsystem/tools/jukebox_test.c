// jukebox_test.c — Minimal standalone jukebox song tester
// Plays songs through the bridge's SoundSynth API without the full game.
//
// Build: make jukebox-test
// Usage: ./build/bin/jukebox-test [song_index]
//        ./build/bin/jukebox-test --list
//        ./build/bin/jukebox-test --headless <song_index> [seconds]
//
// Controls: LEFT/RIGHT = prev/next, SPACE = stop, L = toggle log, Q/ESC = quit

#include "raylib.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#include "src/sound/sound_synth_bridge.h"

int main(int argc, char* argv[]) {
    // --list: just print songs and exit (need audio init for the bridge)
    bool listOnly = (argc > 1 && strcmp(argv[1], "--list") == 0);
    // --headless <song> [seconds]: play song, tick, dump log, exit (no window)
    bool headless = (argc > 2 && strcmp(argv[1], "--headless") == 0);

    int startSong = -1;
    float headlessDuration = 30.0f;  // default 30s

    if (headless) {
        startSong = atoi(argv[2]);
        if (argc > 3) headlessDuration = (float)atof(argv[3]);
    } else if (argc > 1 && !listOnly) {
        startSong = atoi(argv[1]);
    }

    // Minimal window (needed for raylib audio) — or headless audio only
    if (!listOnly && !headless) {
        SetConfigFlags(FLAG_WINDOW_RESIZABLE);
        InitWindow(480, 140, "Jukebox Test");
        SetTargetFPS(30);
    } else {
        // Need audio device for bridge init (no window)
        InitAudioDevice();
    }

    // Create and init sound system
    SoundSynth* synth = SoundSynthCreate();
    SoundSynthInitAudio(synth, 44100, 512);

    int songCount = SoundSynthGetSongCount();

    if (listOnly) {
        printf("Available songs (%d):\n", songCount);
        for (int i = 0; i < songCount; i++) {
            printf("  %2d: %s\n", i, SoundSynthGetSongName(i));
        }
        SoundSynthDestroy(synth);
        CloseAudioDevice();
        return 0;
    }

    // --- Headless mode: enable log, play song, tick, dump, exit ---
    if (headless) {
        if (startSong < 0 || startSong >= songCount) {
            printf("Song index %d out of range (0-%d)\n", startSong, songCount - 1);
            SoundSynthDestroy(synth);
            CloseAudioDevice();
            return 1;
        }

        SoundSynthLogToggle();  // enable log
        printf("Playing \"%s\" (index %d) for %.1fs headless...\n",
               SoundSynthGetSongName(startSong), startSong, headlessDuration);
        SoundSynthJukeboxPlay(synth, startSong);

        // Simulate ticks at ~30fps
        float dt = 1.0f / 30.0f;
        float elapsed = 0.0f;
        while (elapsed < headlessDuration) {
            SoundSynthUpdate(synth, dt);
            usleep(33333);  // ~30fps real-time so audio callback runs
            elapsed += dt;
        }

        SoundSynthStopSong(synth);
        SoundSynthLogDump();
        printf("Done. Log dumped to daw_sound.log\n");
        SoundSynthDestroy(synth);
        CloseAudioDevice();
        return 0;
    }

    // --- Interactive mode ---
    int currentSong = 0;
    if (startSong >= 0 && startSong < songCount) {
        currentSong = startSong;
        SoundSynthJukeboxPlay(synth, currentSong);
    }

    bool logEnabled = false;

    while (!WindowShouldClose()) {
        // Controls
        if (IsKeyPressed(KEY_RIGHT) || IsKeyPressed(KEY_N)) {
            currentSong = (currentSong + 1) % songCount;
            SoundSynthJukeboxPlay(synth, currentSong);
        }
        if (IsKeyPressed(KEY_LEFT) || IsKeyPressed(KEY_P)) {
            currentSong = (currentSong - 1 + songCount) % songCount;
            SoundSynthJukeboxPlay(synth, currentSong);
        }
        if (IsKeyPressed(KEY_SPACE)) {
            if (SoundSynthIsSongPlaying(synth)) {
                SoundSynthStopSong(synth);
            } else {
                SoundSynthJukeboxPlay(synth, currentSong);
            }
        }
        if (IsKeyPressed(KEY_L)) {
            SoundSynthLogToggle();
            logEnabled = SoundSynthLogIsEnabled();
            if (logEnabled) printf("Sound log: ON\n");
            else printf("Sound log: OFF\n");
        }
        if (IsKeyPressed(KEY_D)) {
            SoundSynthLogDump();
            printf("Log dumped to daw_sound.log\n");
        }
        if (IsKeyPressed(KEY_Q)) break;

        // Number keys 0-9 for quick select
        for (int k = 0; k <= 9 && k < songCount; k++) {
            if (IsKeyPressed(KEY_ZERO + k)) {
                currentSong = k;
                SoundSynthJukeboxPlay(synth, currentSong);
            }
        }

        // Update (needed for song player pattern cycling)
        SoundSynthUpdate(synth, GetFrameTime());

        // Draw minimal UI
        BeginDrawing();
        ClearBackground((Color){30, 30, 35, 255});

        const char* name = SoundSynthGetSongName(currentSong);
        bool playing = SoundSynthIsSongPlaying(synth);

        DrawText(TextFormat("[%d/%d] %s", currentSong + 1, songCount, name),
                 10, 10, 20, WHITE);
        DrawText(playing ? "PLAYING" : "STOPPED",
                 10, 40, 16, playing ? GREEN : GRAY);
        DrawText(logEnabled ? "LOG: ON" : "LOG: off",
                 10, 60, 12, logEnabled ? YELLOW : DARKGRAY);

        DrawText("LEFT/RIGHT = prev/next    SPACE = play/stop    0-9 = select",
                 10, 90, 10, DARKGRAY);
        DrawText("L = toggle log    D = dump log    Q = quit",
                 10, 105, 10, DARKGRAY);

        // Song list
        for (int i = 0; i < songCount && i < 16; i++) {
            Color c = (i == currentSong) ? WHITE : (Color){60, 60, 70, 255};
            DrawText(TextFormat("%d", i % 10), 420 + (i / 8) * 25, 10 + (i % 8) * 14, 10, c);
        }

        EndDrawing();
    }

    SoundSynthStopSong(synth);
    SoundSynthDestroy(synth);
    CloseWindow();
    return 0;
}
