#ifndef CUTSCENE_H
#define CUTSCENE_H

#include <stdbool.h>
#include "../../vendor/raylib.h"

typedef struct {
    const char* asciiArt;       // Sprite art (drawn first, fixed grid, no typewriter, NULL = none)
    const char* text;           // Font text (drawn on top, typewriter effect, supports \n)
    float typewriterSpeed;      // chars per second (0 = instant)
    bool dropCap;               // First letter drawn large with background block
} Panel;

typedef enum {
    CUTSCENE_NONE,
    CUTSCENE_INTRO,
    CUTSCENE_GAME_OVER,
} CutsceneContext;

typedef struct {
    bool active;
    const Panel* panels;
    int panelCount;
    int currentPanel;
    int revealedArtLines;       // How many ASCII art lines are visible
    int artLineCount;           // Total lines in current panel's asciiArt
    int revealedChars;          // How many text bytes are visible (advances word-by-word)
    float timer;
    bool skipTypewriter;        // true when user pressed SPACE to skip typing
    float charSoundDuration;    // How long to wait before revealing next chunk
    CutsceneContext context;    // Why this cutscene is playing
} CutsceneState;

// Global state
extern CutsceneState cutsceneState;
extern Font* g_cutscene_font;

// Core functions
void InitCutscene(const Panel* panels, int count);
void UpdateCutscene(float dt);
void RenderCutscene(void);
void CloseCutscene(void);

// Convenience
bool IsCutsceneActive(void);

// Test cutscene (hardcoded intro)
void PlayTestCutscene(void);

// Game mode cutscenes
void PlaySurvivalIntroCutscene(void);
void PlayGameOverCutscene(void);

#endif
