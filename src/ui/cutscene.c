#include "cutscene.h"
#include "../../vendor/raylib.h"
#include "../sound/sound_synth_bridge.h"
#include "../sound/sound_phrase.h"
#include <string.h>
#include <stdio.h>
#include <ctype.h>

// Global font from main.c
extern Font comicFont;

// Global sound synth from main.c
extern SoundSynth* soundDebugSynth;

CutsceneState cutsceneState = {0};

// Ensure audio is initialized for cutscene sounds
static void EnsureCutsceneAudio(void) {
    static bool audioInitialized = false;
    if (audioInitialized) return;

    if (!soundDebugSynth) {
        soundDebugSynth = SoundSynthCreate();
    }
    if (soundDebugSynth) {
        SoundSynthInitAudio(soundDebugSynth, 44100, 512);
        audioInitialized = true;
    }
}

// Play a sound for a character being typed
// Returns the duration of the sound (0 if no sound played)
static float PlayCharacterSound(char c) {
    EnsureCutsceneAudio();
    if (!soundDebugSynth) return 0.02f;  // Default short pause

    // Skip whitespace and punctuation (short pause)
    if (!isalpha(c)) return 0.02f;

    char lowerC = tolower(c);
    bool isVowel = (lowerC == 'a' || lowerC == 'e' || lowerC == 'i' ||
                    lowerC == 'o' || lowerC == 'u');

    // Only play sounds for vowels
    if (!isVowel) return 0.02f;  // Consonants: short pause, no sound

    // Use character value for deterministic variation
    int seed = (int)c + cutsceneState.revealedChars;

    SoundToken token = {0};

    // Vowels: mostly normal, occasionally bird chirp
    bool useBird = (seed % 5) == 0;  // 20% bird chirps

    if (useBird) {
        // Short bird chirp (wordy, not whistle-y)
        token.kind = SOUND_TOKEN_BIRD;
        token.variant = (uint8_t)(seed % 8);
        token.freq = 250.0f + (seed % 10) * 25.0f;  // 250-475 Hz
        token.duration = 0.06f + ((seed % 4) * 0.02f);  // 0.06-0.12s
        token.intensity = 0.35f + ((seed % 6) * 0.05f);  // 0.35-0.6
        token.shape = 0.3f + ((seed % 7) * 0.1f);  // 0.3-0.9
    } else {
        // Normal vowel sound (80% of vowels)
        token.kind = SOUND_TOKEN_VOWEL;
        token.variant = (uint8_t)(lowerC % 5);
        float baseFreq = 200.0f + (lowerC % 5) * 30.0f;
        token.freq = baseFreq + ((seed % 7) - 3) * 5.0f;  // ±15 Hz
        token.duration = 0.08f;
        token.intensity = 0.35f + ((seed % 5) * 0.02f);  // 0.35-0.43
        token.shape = 0.4f + ((seed % 4) * 0.1f);  // 0.4-0.7
    }

    token.gap = 0.0f;

    SoundSynthPlayToken(soundDebugSynth, &token);

    return token.duration;  // Return duration so text can wait
}

// Hardcoded test panels (intro sequence)
static Panel testPanels[] = {
    {
        .text =
            "A mover awakens in the wild.\n\n"
            "No tools. No shelter.\n"
            "Only hands and hunger.",
        .typewriterSpeed = 30.0f,
    },
    {
        .text =
            "Gather grass. Gather sticks.\n"
            "Gather stones from the river.\n\n"
            "Survival begins with\n"
            "what the land offers.\n\n"
            "[Press W to Work]",
        .typewriterSpeed = 40.0f,
    }
};

void InitCutscene(const Panel* panels, int count) {
    cutsceneState.active = true;
    cutsceneState.panels = panels;
    cutsceneState.panelCount = count;
    cutsceneState.currentPanel = 0;
    cutsceneState.revealedChars = 0;
    cutsceneState.timer = 0.0f;
    cutsceneState.skipTypewriter = false;
    cutsceneState.charSoundDuration = 0.0f;
}

void UpdateCutscene(float dt) {
    if (!cutsceneState.active) return;

    const Panel* panel = &cutsceneState.panels[cutsceneState.currentPanel];
    int textLen = (int)strlen(panel->text);

    // Typewriter effect (sound-synced: each char waits for its sound to finish)
    if (!cutsceneState.skipTypewriter && cutsceneState.revealedChars < textLen) {
        cutsceneState.timer += dt;

        // Wait for current character's sound to finish before revealing next
        if (cutsceneState.timer >= cutsceneState.charSoundDuration) {
            // Reveal next character
            cutsceneState.revealedChars++;
            if (cutsceneState.revealedChars > textLen) {
                cutsceneState.revealedChars = textLen;
            }

            // Play sound for this character and get its duration
            if (cutsceneState.revealedChars <= textLen) {
                char c = panel->text[cutsceneState.revealedChars - 1];
                cutsceneState.charSoundDuration = PlayCharacterSound(c);
                cutsceneState.timer = 0.0f;  // Reset timer for next character
            }
        }
    }

    // Input: SPACE advances or skips typewriter
    if (IsKeyPressed(KEY_SPACE) || IsKeyPressed(KEY_ENTER)) {
        if (cutsceneState.revealedChars < textLen) {
            // Skip typewriter, reveal all
            cutsceneState.revealedChars = textLen;
            cutsceneState.skipTypewriter = true;
        } else {
            // Advance to next panel
            cutsceneState.currentPanel++;
            if (cutsceneState.currentPanel >= cutsceneState.panelCount) {
                CloseCutscene();
            } else {
                cutsceneState.revealedChars = 0;
                cutsceneState.timer = 0.0f;
                cutsceneState.skipTypewriter = false;
                cutsceneState.charSoundDuration = 0.0f;
            }
        }
    }

    // ESC or Q: skip entire cutscene
    if (IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_Q)) {
        CloseCutscene();
    }
}

void RenderCutscene(void) {
    if (!cutsceneState.active) return;

    const Panel* panel = &cutsceneState.panels[cutsceneState.currentPanel];

    int screenW = GetScreenWidth();
    int screenH = GetScreenHeight();

    // Semi-transparent overlay (dims the game behind)
    DrawRectangle(0, 0, screenW, screenH, (Color){0, 0, 0, 180});

    // Calculate panel dimensions (centered popup, ~70% of screen)
    int panelW = (int)(screenW * 0.7f);
    int panelH = (int)(screenH * 0.5f);
    int panelX = (screenW - panelW) / 2;
    int panelY = (screenH - panelH) / 2;

    // Panel background (solid dark)
    DrawRectangle(panelX, panelY, panelW, panelH, (Color){20, 20, 25, 255});

    // Double border (outer + inner)
    DrawRectangleLinesEx((Rectangle){panelX, panelY, panelW, panelH}, 3, RAYWHITE);
    DrawRectangleLinesEx((Rectangle){panelX + 8, panelY + 8, panelW - 16, panelH - 16}, 2, GRAY);

    // Text with typewriter effect
    char buffer[2048];
    int len = cutsceneState.revealedChars;
    if (len > 2047) len = 2047;
    strncpy(buffer, panel->text, len);
    buffer[len] = '\0';

    // Draw text (centered in panel)
    int textX = panelX + 40;
    int textY = panelY + panelH / 2 - 60;  // Centered vertically in panel
    int fontSize = 24;
    int spacing = 2;

    // Use Comic Sans BMFont
    DrawTextEx(comicFont, buffer, (Vector2){textX, textY}, fontSize, spacing, RAYWHITE);

    // Show prompt when text fully revealed
    bool textFullyRevealed = cutsceneState.revealedChars >= (int)strlen(panel->text);
    if (textFullyRevealed) {
        const char* prompt = "[SPACE] to continue";
        Vector2 promptSize = MeasureTextEx(comicFont, prompt, 20, 2);
        int promptX = panelX + panelW - (int)promptSize.x - 20;
        int promptY = panelY + panelH - 35;
        DrawTextEx(comicFont, prompt, (Vector2){promptX, promptY}, 20, 2, GRAY);
    }

    // Panel counter (bottom left of panel)
    char counter[32];
    snprintf(counter, sizeof(counter), "%d / %d",
             cutsceneState.currentPanel + 1, cutsceneState.panelCount);
    DrawTextEx(comicFont, counter, (Vector2){panelX + 20, panelY + panelH - 35}, 20, 2, DARKGRAY);
}

void CloseCutscene(void) {
    cutsceneState.active = false;
}

bool IsCutsceneActive(void) {
    return cutsceneState.active;
}

void PlayTestCutscene(void) {
    InitCutscene(testPanels, 2);
}
