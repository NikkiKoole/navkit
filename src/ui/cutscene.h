#ifndef CUTSCENE_H
#define CUTSCENE_H

#include <stdbool.h>

typedef struct {
    const char* text;           // Text to display (supports \n for newlines)
    float typewriterSpeed;      // chars per second (0 = instant)
} Panel;

typedef struct {
    bool active;
    const Panel* panels;
    int panelCount;
    int currentPanel;
    int revealedChars;
    float timer;
    bool skipTypewriter;        // true when user pressed SPACE to skip typing
    float charSoundDuration;    // How long to wait for current character's sound
} CutsceneState;

// Global state
extern CutsceneState cutsceneState;

// Core functions
void InitCutscene(const Panel* panels, int count);
void UpdateCutscene(float dt);
void RenderCutscene(void);
void CloseCutscene(void);

// Convenience
bool IsCutsceneActive(void);

// Test cutscene (hardcoded intro)
void PlayTestCutscene(void);

#endif
