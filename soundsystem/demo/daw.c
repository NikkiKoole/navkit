// PixelSynth DAW - Page-based UI skeleton
// F1=Synth  F2=Drums  F3=Seq  F4=Mix  F5=Song
// Transport bar persistent across all pages

#include "../../vendor/raylib.h"
#include "../../assets/fonts/comic_embedded.h"
#define UI_IMPLEMENTATION
#include "../../shared/ui.h"
#include <math.h>
#include <string.h>

#define SCREEN_WIDTH 1200
#define SCREEN_HEIGHT 800

// ============================================================================
// PAGES
// ============================================================================

typedef enum {
    PAGE_SYNTH,
    PAGE_DRUMS,
    PAGE_SEQ,
    PAGE_MIX,
    PAGE_SONG,
    PAGE_COUNT
} Page;

static const char* pageNames[PAGE_COUNT] = {"Synth", "Drums", "Seq", "Mix", "Song"};
static const int pageKeys[PAGE_COUNT] = {KEY_F1, KEY_F2, KEY_F3, KEY_F4, KEY_F5};
static Page currentPage = PAGE_SYNTH;

// ============================================================================
// LAYOUT CONSTANTS
// ============================================================================

// Transport bar at top
#define TRANSPORT_H 40
#define TRANSPORT_Y 0

// Tab bar just below transport
#define TAB_H 28
#define TAB_Y (TRANSPORT_H)

// Main content area
#define CONTENT_Y (TRANSPORT_H + TAB_H + 4)
#define CONTENT_H (SCREEN_HEIGHT - CONTENT_Y)

// Sequencer grid (visible on SEQ and DRUMS pages, bottom portion)
#define SEQ_GRID_H 200

// ============================================================================
// PLACEHOLDER STATE (will be replaced by real soundsystem state)
// ============================================================================

// Transport
static bool playing = false;
static float bpm = 120.0f;
static int currentPattern = 0;

// Synth page
static float attack = 0.01f, decay = 0.1f, sustain = 0.5f, release = 0.3f;
static float filterCutoff = 1.0f, filterReso = 0.0f;
static float vibratoRate = 5.0f, vibratoDepth = 0.0f;
static int waveType = 0;
static const char* waveNames[] = {"Square", "Saw", "Triangle", "Noise", "SCW",
    "Voice", "Pluck", "Additive", "Mallet", "Granular", "FM", "PD", "Membrane", "Bird"};
static int patchSlot = 0;
static const char* patchNames[] = {"Preview", "Bass", "Lead", "Chord"};

// Drums page
static float drumVolume = 0.6f;
static float kickPitch = 50.0f, kickDecay = 0.5f;
static float snarePitch = 180.0f, snareDecay = 0.2f, snareSnappy = 0.6f;
static float hhClosed = 0.05f, hhOpen = 0.4f;
static bool sidechainOn = false;

// Mix page
static float busVolumes[7] = {0.8f, 0.8f, 0.8f, 0.8f, 0.8f, 0.8f, 0.8f};
static float masterVol = 0.8f;
static bool busMute[7] = {false};
static bool busSolo[7] = {false};
static const char* busNames[7] = {"Drum0", "Drum1", "Drum2", "Drum3", "Bass", "Lead", "Chord"};
static int selectedBus = -1;

// Effects (shown on mix page when bus selected)
static bool distOn = false, delayOn = false, reverbOn = false;
static float distDrive = 2.0f, distTone = 0.7f, distMix = 0.5f;
static float delayTime = 0.3f, delayFeedback = 0.4f, delayMix = 0.3f;
static float reverbSize = 0.5f, reverbDamp = 0.5f, reverbMix = 0.3f;

// Song page
static int chainLength = 0;
static int chain[64] = {0};
static int sceneCount = 4;

// LFOs
static float filterLfoRate = 0.0f, filterLfoDepth = 0.0f;
static int filterLfoShape = 0;
static float ampLfoRate = 0.0f, ampLfoDepth = 0.0f;
static int ampLfoShape = 0;
static const char* lfoShapeNames[] = {"Sine", "Tri", "Sqr", "Saw", "S&H"};

// ============================================================================
// DRAW: TRANSPORT BAR (always visible)
// ============================================================================

static void drawTransport(void) {
    // Background
    DrawRectangle(0, TRANSPORT_Y, SCREEN_WIDTH, TRANSPORT_H, (Color){25, 25, 30, 255});
    DrawLine(0, TRANSPORT_H - 1, SCREEN_WIDTH, TRANSPORT_H - 1, (Color){60, 60, 70, 255});

    int y = TRANSPORT_Y + 10;
    float x = 15;

    // Title
    DrawTextShadow("PixelSynth", (int)x, y, 22, WHITE);
    x += 170;

    // Play / Stop
    if (PushButton(x, y, playing ? "Stop" : "Play")) {
        playing = !playing;
    }
    x += 80;

    // BPM
    DraggableFloat(x, y, "BPM", &bpm, 2.0f, 60.0f, 200.0f);
    x += 160;

    // Pattern indicator
    DrawTextShadow(TextFormat("Pat: %d", currentPattern + 1), (int)x, y, 18, LIGHTGRAY);
    x += 80;

    // Playing indicator
    if (playing) {
        DrawTextShadow(">>>", (int)x, y, 18, GREEN);
    }

    // Right side: FPS
    DrawTextShadow(TextFormat("FPS: %d", GetFPS()), SCREEN_WIDTH - 80, y, 12, GRAY);
}

// ============================================================================
// DRAW: TAB BAR
// ============================================================================

static void drawTabBar(void) {
    DrawRectangle(0, TAB_Y, SCREEN_WIDTH, TAB_H, (Color){30, 32, 38, 255});
    DrawLine(0, TAB_Y + TAB_H - 1, SCREEN_WIDTH, TAB_Y + TAB_H - 1, (Color){60, 60, 70, 255});

    Vector2 mouse = GetMousePosition();
    int tabW = 90;
    int tabSpacing = 4;
    int startX = 15;

    for (int i = 0; i < PAGE_COUNT; i++) {
        int tx = startX + i * (tabW + tabSpacing);
        int ty = TAB_Y + 3;
        Rectangle tabRect = {(float)tx, (float)ty, (float)tabW, (float)(TAB_H - 6)};
        bool hovered = CheckCollisionPointRec(mouse, tabRect);
        bool active = (i == (int)currentPage);

        Color bg = active ? (Color){60, 65, 80, 255} : (Color){40, 42, 50, 255};
        if (hovered && !active) bg = (Color){50, 52, 62, 255};

        DrawRectangleRec(tabRect, bg);
        if (active) {
            DrawRectangle(tx, ty + TAB_H - 8, tabW, 2, ORANGE);
        }
        DrawRectangleLinesEx(tabRect, 1, active ? ORANGE : (Color){55, 55, 65, 255});

        // Label with F-key hint
        Color textCol = active ? WHITE : (hovered ? LIGHTGRAY : GRAY);
        DrawTextShadow(TextFormat("F%d %s", i + 1, pageNames[i]), tx + 8, ty + 4, 14, textCol);

        if (hovered && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            currentPage = (Page)i;
            ui_consume_click();
        }
    }
}

// ============================================================================
// DRAW: SYNTH PAGE
// ============================================================================

static void drawPageSynth(void) {
    int y = CONTENT_Y + 10;

    // Left panel: Wave type + wave-specific params
    UIColumn col1 = ui_column(20, y, 20);
    ui_col_sublabel(&col1, "Patch:", ORANGE);
    ui_col_cycle(&col1, "Slot", patchNames, 4, &patchSlot);
    ui_col_space(&col1, 8);
    ui_col_sublabel(&col1, "Oscillator:", ORANGE);
    ui_col_cycle(&col1, "Wave", waveNames, 14, &waveType);
    ui_col_space(&col1, 8);

    // Wave-specific params would go here (conditionally shown based on waveType)
    // For the skeleton, show a placeholder
    DrawTextShadow("(wave-specific params here)", 20, (int)col1.y, 12, (Color){80, 80, 80, 255});

    // Middle panel: Envelope + filter
    UIColumn col2 = ui_column(250, y, 20);
    ui_col_sublabel(&col2, "Envelope:", ORANGE);
    ui_col_float(&col2, "Attack", &attack, 0.5f, 0.001f, 2.0f);
    ui_col_float(&col2, "Decay", &decay, 0.5f, 0.0f, 2.0f);
    ui_col_float(&col2, "Sustain", &sustain, 0.5f, 0.0f, 1.0f);
    ui_col_float(&col2, "Release", &release, 0.5f, 0.01f, 3.0f);
    ui_col_space(&col2, 8);
    ui_col_sublabel(&col2, "Filter:", ORANGE);
    ui_col_float(&col2, "Cutoff", &filterCutoff, 0.05f, 0.01f, 1.0f);
    ui_col_float(&col2, "Reso", &filterReso, 0.05f, 0.0f, 1.0f);
    ui_col_space(&col2, 8);
    ui_col_sublabel(&col2, "Vibrato:", ORANGE);
    ui_col_float(&col2, "Rate", &vibratoRate, 0.5f, 0.5f, 15.0f);
    ui_col_float(&col2, "Depth", &vibratoDepth, 0.2f, 0.0f, 2.0f);

    // Right panel: LFOs
    UIColumn col3 = ui_column(480, y, 20);
    ui_col_sublabel(&col3, "Filter LFO:", ORANGE);
    ui_col_float(&col3, "Rate", &filterLfoRate, 0.5f, 0.0f, 20.0f);
    ui_col_float(&col3, "Depth", &filterLfoDepth, 0.05f, 0.0f, 2.0f);
    ui_col_cycle(&col3, "Shape", lfoShapeNames, 5, &filterLfoShape);
    ui_col_space(&col3, 8);
    ui_col_sublabel(&col3, "Amp LFO:", ORANGE);
    ui_col_float(&col3, "Rate", &ampLfoRate, 0.5f, 0.0f, 20.0f);
    ui_col_float(&col3, "Depth", &ampLfoDepth, 0.05f, 0.0f, 1.0f);
    ui_col_cycle(&col3, "Shape", lfoShapeNames, 5, &ampLfoShape);

    // Big empty area on the right: future waveform display / keyboard
    int vizX = 720;
    int vizY = CONTENT_Y + 10;
    int vizW = SCREEN_WIDTH - vizX - 20;
    int vizH = CONTENT_H - 20;
    DrawRectangle(vizX, vizY, vizW, vizH, (Color){20, 20, 25, 255});
    DrawRectangleLinesEx((Rectangle){(float)vizX, (float)vizY, (float)vizW, (float)vizH}, 1, (Color){50, 50, 60, 255});
    DrawTextShadow("Waveform / Keyboard", vizX + 10, vizY + 10, 14, (Color){60, 60, 70, 255});
}

// ============================================================================
// DRAW: DRUMS PAGE
// ============================================================================

static void drawPageDrums(void) {
    int y = CONTENT_Y + 10;

    // Left: drum sound params
    UIColumn col1 = ui_column(20, y, 20);
    ui_col_sublabel(&col1, "Master:", ORANGE);
    ui_col_float(&col1, "Volume", &drumVolume, 0.05f, 0.0f, 1.0f);
    ui_col_space(&col1, 8);
    ui_col_sublabel(&col1, "Kick:", ORANGE);
    ui_col_float(&col1, "Pitch", &kickPitch, 3.0f, 30.0f, 100.0f);
    ui_col_float(&col1, "Decay", &kickDecay, 0.07f, 0.1f, 1.5f);
    ui_col_space(&col1, 8);
    ui_col_sublabel(&col1, "Snare:", ORANGE);
    ui_col_float(&col1, "Pitch", &snarePitch, 10.0f, 100.0f, 350.0f);
    ui_col_float(&col1, "Decay", &snareDecay, 0.03f, 0.05f, 0.6f);
    ui_col_float(&col1, "Snappy", &snareSnappy, 0.05f, 0.0f, 1.0f);
    ui_col_space(&col1, 8);
    ui_col_sublabel(&col1, "HiHat:", ORANGE);
    ui_col_float(&col1, "Closed", &hhClosed, 0.01f, 0.01f, 0.2f);
    ui_col_float(&col1, "Open", &hhOpen, 0.05f, 0.1f, 1.0f);
    ui_col_space(&col1, 8);
    ui_col_sublabel(&col1, "Sidechain:", ORANGE);
    ui_col_toggle(&col1, "On", &sidechainOn);

    // Right: drum grid (placeholder)
    int gridX = 250;
    int gridY = CONTENT_Y + 10;
    int gridW = SCREEN_WIDTH - gridX - 20;
    int gridH = CONTENT_H - 20;

    DrawRectangle(gridX, gridY, gridW, gridH, (Color){20, 20, 25, 255});
    DrawRectangleLinesEx((Rectangle){(float)gridX, (float)gridY, (float)gridW, (float)gridH}, 1, (Color){50, 50, 60, 255});

    // Placeholder drum grid: 4 tracks x 16 steps
    int cellW = (gridW - 100) / 16;
    int cellH = 28;
    int labelW = 80;
    const char* trackNames[] = {"Kick", "Snare", "CH", "Clap"};
    static bool steps[4][16] = {{0}};

    // Initialize with a basic pattern
    static bool initialized = false;
    if (!initialized) {
        steps[0][0] = steps[0][4] = steps[0][8] = steps[0][12] = true;  // four-on-floor kick
        steps[1][4] = steps[1][12] = true;  // snare on 2 and 4
        steps[2][0] = steps[2][2] = steps[2][4] = steps[2][6] = true;  // hi-hat eighths
        steps[2][8] = steps[2][10] = steps[2][12] = steps[2][14] = true;
        initialized = true;
    }

    Vector2 mouse = GetMousePosition();

    for (int track = 0; track < 4; track++) {
        int ty = gridY + 40 + track * (cellH + 4);
        DrawTextShadow(trackNames[track], gridX + 10, ty + 6, 14, LIGHTGRAY);

        for (int step = 0; step < 16; step++) {
            int sx = gridX + labelW + step * cellW;
            Rectangle cell = {(float)sx, (float)ty, (float)(cellW - 2), (float)cellH};
            bool active = steps[track][step];
            bool hovered = CheckCollisionPointRec(mouse, cell);

            Color bg = (step / 4) % 2 == 0 ? (Color){40, 40, 45, 255} : (Color){32, 32, 36, 255};
            if (active) bg = (Color){50, 130, 70, 255};
            if (hovered) { bg.r += 25; bg.g += 25; bg.b += 25; }

            DrawRectangleRec(cell, bg);
            DrawRectangleLinesEx(cell, 1, (Color){60, 60, 65, 255});

            if (hovered && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                steps[track][step] = !steps[track][step];
                ui_consume_click();
            }
        }
    }

    // Beat markers
    for (int i = 0; i < 4; i++) {
        int bx = gridX + labelW + i * 4 * cellW + 2;
        DrawTextShadow(TextFormat("%d", i + 1), bx, gridY + 22, 12, GRAY);
    }
}

// ============================================================================
// DRAW: SEQ PAGE (future piano roll lives here)
// ============================================================================

static void drawPageSeq(void) {
    int y = CONTENT_Y + 10;

    // Pattern bar
    int patW = 32;
    int patH = 24;
    int patX = 20;
    DrawTextShadow("Pattern:", patX, y + 4, 14, YELLOW);
    patX += 80;

    Vector2 mouse = GetMousePosition();
    for (int i = 0; i < 8; i++) {
        Rectangle r = {(float)(patX + i * (patW + 4)), (float)y, (float)patW, (float)patH};
        bool hovered = CheckCollisionPointRec(mouse, r);
        bool active = (i == currentPattern);

        Color bg = active ? (Color){60, 100, 60, 255} : (Color){40, 42, 50, 255};
        if (hovered) { bg.r += 25; bg.g += 25; bg.b += 25; }

        DrawRectangleRec(r, bg);
        DrawRectangleLinesEx(r, 1, active ? GREEN : (Color){60, 60, 70, 255});
        DrawTextShadow(TextFormat("%d", i + 1), patX + i * (patW + 4) + 11, y + 5, 14,
                       active ? WHITE : GRAY);

        if (hovered && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            currentPattern = i;
            ui_consume_click();
        }
    }

    // Main area: piano roll placeholder
    int pianoX = 20;
    int pianoY = y + patH + 20;
    int pianoW = SCREEN_WIDTH - 40;
    int pianoH = CONTENT_H - patH - 40;

    DrawRectangle(pianoX, pianoY, pianoW, pianoH, (Color){20, 20, 25, 255});
    DrawRectangleLinesEx((Rectangle){(float)pianoX, (float)pianoY, (float)pianoW, (float)pianoH}, 1, (Color){50, 50, 60, 255});

    // Piano key labels on left
    int keyW = 40;
    int noteH = pianoH / 24;  // Show 2 octaves
    const char* noteNames[] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
    for (int i = 0; i < 24; i++) {
        int ny = pianoY + pianoH - (i + 1) * noteH;
        int noteIdx = i % 12;
        int octave = 3 + i / 12;
        bool isBlack = (noteIdx == 1 || noteIdx == 3 || noteIdx == 6 || noteIdx == 8 || noteIdx == 10);

        Color keyColor = isBlack ? (Color){30, 30, 35, 255} : (Color){45, 45, 50, 255};
        DrawRectangle(pianoX, ny, keyW, noteH - 1, keyColor);
        DrawTextShadow(TextFormat("%s%d", noteNames[noteIdx], octave), pianoX + 3, ny + 1, 9, GRAY);

        // Grid lines across the roll
        Color lineCol = (noteIdx == 0) ? (Color){60, 60, 70, 255} : (Color){35, 35, 40, 255};
        DrawLine(pianoX + keyW, ny + noteH - 1, pianoX + pianoW, ny + noteH - 1, lineCol);
    }

    // Beat grid lines (vertical)
    int stepsVisible = 16;
    int stepW = (pianoW - keyW) / stepsVisible;
    for (int i = 0; i <= stepsVisible; i++) {
        int lx = pianoX + keyW + i * stepW;
        Color lineCol = (i % 4 == 0) ? (Color){70, 70, 80, 255} : (Color){40, 40, 48, 255};
        DrawLine(lx, pianoY, lx, pianoY + pianoH, lineCol);
    }

    DrawTextShadow("Piano Roll", pianoX + pianoW / 2 - 40, pianoY + pianoH / 2 - 8, 16, (Color){60, 60, 70, 255});
}

// ============================================================================
// DRAW: MIX PAGE
// ============================================================================

static void drawPageMix(void) {
    int y = CONTENT_Y + 10;
    Vector2 mouse = GetMousePosition();

    // Channel strips
    int stripW = 90;
    int stripH = CONTENT_H - 80;
    int stripSpacing = 6;
    int startX = 20;

    for (int b = 0; b < 7; b++) {
        int sx = startX + b * (stripW + stripSpacing);
        int sy = y;
        bool isSelected = (selectedBus == b);
        bool hovered = CheckCollisionPointRec(mouse, (Rectangle){(float)sx, (float)sy, (float)stripW, (float)stripH});

        // Strip background
        Color bg = isSelected ? (Color){45, 50, 60, 255} : (Color){30, 32, 38, 255};
        DrawRectangle(sx, sy, stripW, stripH, bg);
        DrawRectangleLinesEx((Rectangle){(float)sx, (float)sy, (float)stripW, (float)stripH},
                             1, isSelected ? ORANGE : (Color){50, 50, 60, 255});

        // Name
        DrawTextShadow(busNames[b], sx + 8, sy + 6, 12, isSelected ? WHITE : LIGHTGRAY);

        // Volume fader
        int faderX = sx + 10;
        int faderY = sy + 30;
        int faderW = 20;
        int faderH = stripH - 100;
        DrawRectangle(faderX, faderY, faderW, faderH, (Color){20, 20, 25, 255});
        int fillH = (int)(faderH * busVolumes[b]);
        Color volColor = busMute[b] ? (Color){80, 40, 40, 255} : (Color){50, 150, 50, 255};
        DrawRectangle(faderX + 1, faderY + faderH - fillH, faderW - 2, fillH, volColor);
        DrawRectangleLinesEx((Rectangle){(float)faderX, (float)faderY, (float)faderW, (float)faderH},
                             1, (Color){50, 50, 60, 255});

        // Fader drag
        Rectangle faderRect = {(float)faderX, (float)faderY, (float)faderW, (float)faderH};
        if (CheckCollisionPointRec(mouse, faderRect) && IsMouseButtonDown(MOUSE_LEFT_BUTTON)) {
            busVolumes[b] = 1.0f - (mouse.y - faderY) / (float)faderH;
            if (busVolumes[b] < 0.0f) busVolumes[b] = 0.0f;
            if (busVolumes[b] > 1.0f) busVolumes[b] = 1.0f;
        }

        // Mute / Solo buttons
        int btnY = sy + stripH - 60;
        Rectangle muteRect = {(float)(sx + 8), (float)btnY, 30, 18};
        bool muteHovered = CheckCollisionPointRec(mouse, muteRect);
        DrawRectangleRec(muteRect, busMute[b] ? (Color){180, 60, 60, 255} : (muteHovered ? (Color){60, 50, 50, 255} : (Color){40, 35, 35, 255}));
        DrawTextShadow("M", sx + 16, btnY + 3, 12, busMute[b] ? WHITE : GRAY);
        if (muteHovered && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            busMute[b] = !busMute[b];
            ui_consume_click();
        }

        Rectangle soloRect = {(float)(sx + 48), (float)btnY, 30, 18};
        bool soloHovered = CheckCollisionPointRec(mouse, soloRect);
        DrawRectangleRec(soloRect, busSolo[b] ? (Color){180, 180, 60, 255} : (soloHovered ? (Color){60, 60, 50, 255} : (Color){40, 40, 35, 255}));
        DrawTextShadow("S", sx + 56, btnY + 3, 12, busSolo[b] ? BLACK : GRAY);
        if (soloHovered && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            busSolo[b] = !busSolo[b];
            ui_consume_click();
        }

        // Volume label
        DrawTextShadow(TextFormat("%.0f%%", busVolumes[b] * 100), sx + 8, sy + stripH - 35, 11, GRAY);

        // Click to select
        if (hovered && IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && !muteHovered && !soloHovered) {
            selectedBus = (selectedBus == b) ? -1 : b;
            ui_consume_click();
        }
    }

    // Master strip
    int masterX = startX + 7 * (stripW + stripSpacing) + 10;
    DrawRectangle(masterX, y, 60, stripH, (Color){35, 35, 40, 255});
    DrawRectangleLinesEx((Rectangle){(float)masterX, (float)y, 60, (float)stripH}, 1, (Color){80, 80, 90, 255});
    DrawTextShadow("Master", masterX + 6, y + 6, 12, YELLOW);

    int mfX = masterX + 18;
    int mfY = y + 30;
    int mfH = stripH - 100;
    DrawRectangle(mfX, mfY, 20, mfH, (Color){20, 20, 25, 255});
    int mfFill = (int)(mfH * masterVol);
    DrawRectangle(mfX + 1, mfY + mfH - mfFill, 18, mfFill, (Color){180, 160, 60, 255});
    DrawRectangleLinesEx((Rectangle){(float)mfX, (float)mfY, 20, (float)mfH}, 1, (Color){60, 60, 50, 255});

    Rectangle mfRect = {(float)mfX, (float)mfY, 20, (float)mfH};
    if (CheckCollisionPointRec(mouse, mfRect) && IsMouseButtonDown(MOUSE_LEFT_BUTTON)) {
        masterVol = 1.0f - (mouse.y - mfY) / (float)mfH;
        if (masterVol < 0.0f) masterVol = 0.0f;
        if (masterVol > 1.0f) masterVol = 1.0f;
    }

    // Detail panel (right side, when a bus is selected)
    int panelX = masterX + 80;
    int panelW = SCREEN_WIDTH - panelX - 20;
    if (selectedBus >= 0 && panelW > 100) {
        DrawRectangle(panelX, y, panelW, stripH, (Color){35, 38, 42, 255});
        DrawRectangleLinesEx((Rectangle){(float)panelX, (float)y, (float)panelW, (float)stripH}, 1, ORANGE);
        DrawTextShadow(TextFormat("%s Effects", busNames[selectedBus]), panelX + 10, y + 8, 14, ORANGE);

        UIColumn fxCol = ui_column(panelX + 10, y + 35, 20);
        ui_col_sublabel(&fxCol, "Distortion:", ORANGE);
        ui_col_toggle(&fxCol, "On", &distOn);
        if (distOn) {
            ui_col_float(&fxCol, "Drive", &distDrive, 0.5f, 1.0f, 20.0f);
            ui_col_float(&fxCol, "Tone", &distTone, 0.05f, 0.0f, 1.0f);
            ui_col_float(&fxCol, "Mix", &distMix, 0.05f, 0.0f, 1.0f);
        }
        ui_col_space(&fxCol, 8);
        ui_col_sublabel(&fxCol, "Delay:", ORANGE);
        ui_col_toggle(&fxCol, "On", &delayOn);
        if (delayOn) {
            ui_col_float(&fxCol, "Time", &delayTime, 0.05f, 0.05f, 1.0f);
            ui_col_float(&fxCol, "Feedback", &delayFeedback, 0.05f, 0.0f, 0.9f);
            ui_col_float(&fxCol, "Mix", &delayMix, 0.05f, 0.0f, 1.0f);
        }
        ui_col_space(&fxCol, 8);
        ui_col_sublabel(&fxCol, "Reverb:", ORANGE);
        ui_col_toggle(&fxCol, "On", &reverbOn);
        if (reverbOn) {
            ui_col_float(&fxCol, "Size", &reverbSize, 0.05f, 0.0f, 1.0f);
            ui_col_float(&fxCol, "Damping", &reverbDamp, 0.05f, 0.0f, 1.0f);
            ui_col_float(&fxCol, "Mix", &reverbMix, 0.05f, 0.0f, 1.0f);
        }
    } else if (panelW > 100) {
        DrawRectangle(panelX, y, panelW, 40, (Color){30, 32, 36, 255});
        DrawRectangleLinesEx((Rectangle){(float)panelX, (float)y, (float)panelW, 40}, 1, (Color){50, 50, 60, 255});
        DrawTextShadow("Click a bus to edit effects", panelX + 10, y + 12, 14, GRAY);
    }
}

// ============================================================================
// DRAW: SONG PAGE
// ============================================================================

static void drawPageSong(void) {
    int y = CONTENT_Y + 10;
    Vector2 mouse = GetMousePosition();

    // Chain editor
    DrawTextShadow("Pattern Chain:", 20, y, 16, (Color){180, 180, 255, 255});
    y += 25;

    int chW = 32;
    int chH = 28;
    int chX = 20;

    for (int i = 0; i < chainLength; i++) {
        Rectangle r = {(float)(chX + i * (chW + 4)), (float)y, (float)chW, (float)chH};
        bool hovered = CheckCollisionPointRec(mouse, r);
        Color bg = hovered ? (Color){60, 65, 75, 255} : (Color){45, 45, 55, 255};
        DrawRectangleRec(r, bg);
        DrawRectangleLinesEx(r, 1, (Color){70, 70, 80, 255});
        DrawTextShadow(TextFormat("%d", chain[i] + 1), chX + i * (chW + 4) + 11, y + 7, 14, WHITE);

        if (hovered && IsMouseButtonPressed(MOUSE_RIGHT_BUTTON)) {
            // Remove from chain
            for (int j = i; j < chainLength - 1; j++) chain[j] = chain[j + 1];
            chainLength--;
            ui_consume_click();
        }
        if (hovered) {
            float wheel = GetMouseWheelMove();
            if (wheel > 0) chain[i] = (chain[i] + 1) % 8;
            else if (wheel < 0) chain[i] = (chain[i] + 7) % 8;
        }
    }

    // [+] add to chain
    if (chainLength < 64) {
        int addX = chX + chainLength * (chW + 4);
        Rectangle addRect = {(float)addX, (float)y, (float)chW, (float)chH};
        bool addHovered = CheckCollisionPointRec(mouse, addRect);
        DrawRectangleRec(addRect, addHovered ? (Color){70, 70, 80, 255} : (Color){40, 40, 50, 255});
        DrawRectangleLinesEx(addRect, 1, (Color){80, 80, 90, 255});
        DrawTextShadow("+", addX + 11, y + 7, 14, addHovered ? WHITE : GRAY);

        if (addHovered && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            chain[chainLength++] = currentPattern;
            ui_consume_click();
        }
    }

    y += chH + 20;

    // Scenes
    DrawTextShadow("Scenes:", 20, y, 16, (Color){180, 200, 255, 255});
    y += 25;

    for (int i = 0; i < sceneCount; i++) {
        int sx = 20 + i * 80;
        Rectangle r = {(float)sx, (float)y, 70, 40};
        bool hovered = CheckCollisionPointRec(mouse, r);
        DrawRectangleRec(r, hovered ? (Color){55, 60, 70, 255} : (Color){40, 42, 50, 255});
        DrawRectangleLinesEx(r, 1, (Color){70, 70, 80, 255});
        DrawTextShadow(TextFormat("Scene %d", i + 1), sx + 6, y + 12, 12, LIGHTGRAY);
    }

    y += 60;

    // Arrangement timeline placeholder
    int tlX = 20;
    int tlW = SCREEN_WIDTH - 40;
    int tlH = CONTENT_H - (y - CONTENT_Y) - 20;
    DrawRectangle(tlX, y, tlW, tlH, (Color){20, 20, 25, 255});
    DrawRectangleLinesEx((Rectangle){(float)tlX, (float)y, (float)tlW, (float)tlH}, 1, (Color){50, 50, 60, 255});
    DrawTextShadow("Arrangement Timeline", tlX + tlW / 2 - 80, y + tlH / 2 - 8, 16, (Color){60, 60, 70, 255});
}

// ============================================================================
// MAIN
// ============================================================================

int main(void) {
    InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "PixelSynth DAW");
    SetTargetFPS(60);

    Font font = LoadEmbeddedFont();
    ui_init(&font);

    while (!WindowShouldClose()) {
        // F-key page switching
        for (int i = 0; i < PAGE_COUNT; i++) {
            if (IsKeyPressed(pageKeys[i])) {
                currentPage = (Page)i;
            }
        }

        // Space = play/stop
        if (IsKeyPressed(KEY_SPACE)) {
            playing = !playing;
        }

        BeginDrawing();
        ClearBackground((Color){22, 22, 28, 255});
        ui_begin_frame();

        // Always draw transport + tabs
        drawTransport();
        drawTabBar();

        // Draw current page
        switch (currentPage) {
            case PAGE_SYNTH: drawPageSynth(); break;
            case PAGE_DRUMS: drawPageDrums(); break;
            case PAGE_SEQ:   drawPageSeq(); break;
            case PAGE_MIX:   drawPageMix(); break;
            case PAGE_SONG:  drawPageSong(); break;
            default: break;
        }

        ui_update();
        EndDrawing();
    }

    UnloadFont(font);
    CloseWindow();
    return 0;
}
