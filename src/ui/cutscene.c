#include "cutscene.h"
#include "../../vendor/raylib.h"
#include "../sound/sound_synth_bridge.h"
#include "../sound/sound_phrase.h"
#include "../../assets/atlas8x8.h"
#include "../game_state.h"         // For g_ui_font and other globals
#include "../../shared/ui.h"       // For DrawTextShadow, g_ui_font
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <stdint.h>

// ---------------------------------------------------------------------------
// Warm Vintage Color Palette (inspired by Writing Poet Emacs theme)
// ---------------------------------------------------------------------------
#define COLOR_CREAM        (Color){242, 229, 215, 255}  // Warm paper background
#define COLOR_PARCHMENT    (Color){232, 220, 200, 255}  // Slightly darker panel
#define COLOR_INK          (Color){61, 61, 61, 255}     // Dark charcoal text
#define COLOR_RUST         (Color){160, 82, 45, 255}    // Rust/terracotta headings
#define COLOR_CLAY         (Color){139, 69, 19, 255}    // Dark brown borders
#define COLOR_SLATE_BLUE   (Color){100, 149, 237, 255}  // Medium blue accents

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

// ---------------------------------------------------------------------------
// UTF-8 ASCII Art Rendering
// ---------------------------------------------------------------------------

// UTF-8 character to sprite mapping
typedef struct {
    uint32_t codepoint;  // Unicode codepoint (e.g., 0x2588 for █)
    int spriteId;        // SPRITE8X8_* enum value
} UTF8Sprite;

static const UTF8Sprite utf8SpriteMap[] = {
    { 0x2588, SPRITE8X8_full_block },       // █ FULL BLOCK
    { 0x2593, SPRITE8X8_dark_shade },       // ▓ DARK SHADE
    { 0x2592, SPRITE8X8_medium_shade },     // ▒ MEDIUM SHADE
    { 0x2591, SPRITE8X8_light_shade },      // ░ LIGHT SHADE
    { 0x2584, SPRITE8X8_lower_half },       // ▄ LOWER HALF BLOCK
    { 0x2580, SPRITE8X8_upper_half },       // ▀ UPPER HALF BLOCK
    { 0x2500, SPRITE8X8_light_horizontal }, // ─ BOX DRAWINGS LIGHT HORIZONTAL
    { 0x2502, SPRITE8X8_light_vertical },   // │ BOX DRAWINGS LIGHT VERTICAL
    { 0x00B7, SPRITE8X8_middle_dot },       // · MIDDLE DOT
    { 0x00F7, SPRITE8X8_division },         // ÷ DIVISION SIGN
    { 0x25B2, SPRITE8X8_ramp_n },           // ▲ BLACK UP-POINTING TRIANGLE
    { 0x25BC, SPRITE8X8_ramp_s },           // ▼ BLACK DOWN-POINTING TRIANGLE
    { 0x0040, SPRITE8X8_at_sign },          // @ AT SIGN
};
#define UTF8_SPRITE_COUNT (sizeof(utf8SpriteMap) / sizeof(utf8SpriteMap[0]))

// Decode one UTF-8 character, return codepoint and advance pointer
// Returns 0 on error, advances *text by the number of bytes consumed
static uint32_t DecodeUTF8(const char** text) {
    const unsigned char* s = (const unsigned char*)*text;

    if (s[0] == 0) return 0;  // End of string

    // 1-byte ASCII (0xxxxxxx)
    if ((s[0] & 0x80) == 0) {
        *text += 1;
        return s[0];
    }

    // 2-byte sequence (110xxxxx 10xxxxxx)
    if ((s[0] & 0xE0) == 0xC0 && (s[1] & 0xC0) == 0x80) {
        uint32_t cp = ((s[0] & 0x1F) << 6) | (s[1] & 0x3F);
        *text += 2;
        return cp;
    }

    // 3-byte sequence (1110xxxx 10xxxxxx 10xxxxxx)
    if ((s[0] & 0xF0) == 0xE0 && (s[1] & 0xC0) == 0x80 && (s[2] & 0xC0) == 0x80) {
        uint32_t cp = ((s[0] & 0x0F) << 12) | ((s[1] & 0x3F) << 6) | (s[2] & 0x3F);
        *text += 3;
        return cp;
    }

    // 4-byte sequence (11110xxx 10xxxxxx 10xxxxxx 10xxxxxx)
    if ((s[0] & 0xF8) == 0xF0 && (s[1] & 0xC0) == 0x80 &&
        (s[2] & 0xC0) == 0x80 && (s[3] & 0xC0) == 0x80) {
        uint32_t cp = ((s[0] & 0x07) << 18) | ((s[1] & 0x3F) << 12) |
                      ((s[2] & 0x3F) << 6) | (s[3] & 0x3F);
        *text += 4;
        return cp;
    }

    // Invalid UTF-8, skip one byte
    *text += 1;
    return 0;
}

// Look up sprite ID for a codepoint, returns -1 if not a special character
static int GetSpriteForCodepoint(uint32_t codepoint) {
    for (size_t i = 0; i < UTF8_SPRITE_COUNT; i++) {
        if (utf8SpriteMap[i].codepoint == codepoint) {
            return utf8SpriteMap[i].spriteId;
        }
    }
    return -1;  // Not a special character
}

// Draw text with mixed UTF-8 sprite rendering and regular font
// Sprites are drawn at 8x8, font characters are drawn with specified fontSize
static void DrawTextMixedUTF8(Texture2D atlas, Font font, const char* text,
                              Vector2 position, float fontSize, float spacing, Color tint) {
    float x = position.x;
    float y = position.y;
    const char* p = text;

    // For perfect ASCII art tiling, sprite size must equal line height
    int spriteSize = (int)fontSize;
    float lineHeight = fontSize;  // No extra spacing for vertical tiling

    while (*p) {
        uint32_t codepoint = DecodeUTF8(&p);

        if (codepoint == 0) break;  // End of string or error

        // Check if this is a special sprite character
        int spriteId = GetSpriteForCodepoint(codepoint);

        if (spriteId >= 0) {
            // Draw sprite - tiles must align perfectly for ASCII art
            // Use rust color for ASCII art characters
            Rectangle srcRect = SPRITE8X8GetRect(spriteId);
            Rectangle destRect = { x, y, (float)spriteSize, (float)spriteSize };
            DrawTexturePro(atlas, srcRect, destRect, (Vector2){0, 0}, 0, COLOR_RUST);
            x += spriteSize;  // No spacing between sprites - they must tile seamlessly
        } else {
            // Draw regular character using font
            if (codepoint == '\n') {
                // Newline - use lineHeight for perfect vertical tiling of sprites
                x = position.x;
                y += lineHeight;
            } else if (codepoint == ' ') {
                // Space - just advance without drawing
                x += fontSize * 0.5f;  // Half the font size for space width
            } else if (codepoint >= 32) {  // Printable character
                // Use raylib's DrawTextCodepoint for proper glyph rendering
                int index = GetGlyphIndex(font, codepoint);
                if (font.glyphs != NULL && index >= 0 && index < font.glyphCount) {
                    DrawTextCodepoint(font, codepoint, (Vector2){x, y}, fontSize, tint);

                    // Advance by glyph width
                    GlyphInfo glyph = font.glyphs[index];
                    float advance = (glyph.advanceX == 0) ? font.recs[index].width : glyph.advanceX;
                    x += (advance * fontSize / font.baseSize) + spacing;
                }
            }
        }
    }
}

// Hardcoded test panels (intro sequence)
static Panel testPanels[] = {
    {
        .text =
        "      ·÷·÷·\n"
        "   ░░▒▒▓▓█▓▓▒▒░░\n"
        "  ░▒▒▓▓███████▓▓▒▒░\n"
        " ░▒▓▓▓▓▓▓▓▓▓▓▓▓▓▓▒░\n\n\n"
            "A mover awakens in the wild.\n"
            "No tools. No shelter.\n"
            "Only hands and hunger.",
        .typewriterSpeed = 30.0f,
    },
    {
        .text =
            "    ▄▄▄▄▄▄▄\n"
            "   █░░░░░░█@@\n"
            "   █░░░░░░█ ▲▲▼▼▼░░░▒▒▒▓▓▓███\n"
            "   █░░░░░░█\n"
            "   ▀▀▀▀▀▀▀\n\n"
            "Shading test · light to dark\n"
            "░ ▒ ▓ █  ÷  blocks work!",
        .typewriterSpeed = 40.0f,
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
                // Look backwards to find the start of the current UTF-8 character
                int bytePos = cutsceneState.revealedChars - 1;
                const unsigned char* s = (const unsigned char*)panel->text;

                // Scan back to find UTF-8 character start
                while (bytePos > 0 && (s[bytePos] & 0xC0) == 0x80) {
                    bytePos--;
                }

                // Decode current character
                const char* charStart = panel->text + bytePos;
                const char* tempPos = charStart;
                uint32_t codepoint = DecodeUTF8(&tempPos);
                bool isSpriteChar = (GetSpriteForCodepoint(codepoint) >= 0);

                if (isSpriteChar) {
                    // ASCII ART MODE: Reveal entire line of sprites at once!
                    // Scan forward from current position until we hit a non-sprite or newline
                    const char* scanPos = panel->text + cutsceneState.revealedChars;
                    const char* lineEnd = scanPos;

                    while (*lineEnd && *lineEnd != '\n') {
                        const char* checkPos = lineEnd;
                        uint32_t nextCodepoint = DecodeUTF8(&checkPos);
                        if (nextCodepoint == 0) break;

                        // If we hit a regular text character, stop here
                        if (GetSpriteForCodepoint(nextCodepoint) < 0 && nextCodepoint != ' ' && nextCodepoint != '\t') {
                            break;
                        }
                        lineEnd = checkPos;
                    }

                    // Advance to end of sprite section (or end of line)
                    int chunkSize = (int)(lineEnd - (panel->text + cutsceneState.revealedChars));
                    if (chunkSize > 0) {
                        cutsceneState.revealedChars += chunkSize;
                    }

                    // Tiny delay per line of ASCII art
                    cutsceneState.charSoundDuration = 0.02f;
                } else {
                    // Regular text gets normal sound-based timing
                    char c = panel->text[cutsceneState.revealedChars - 1];
                    cutsceneState.charSoundDuration = PlayCharacterSound(c);
                }

                cutsceneState.timer = 0.0f;
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

    // Semi-transparent warm overlay
    Color overlayColor = COLOR_INK;
    overlayColor.a = 200;
    DrawRectangle(0, 0, screenW, screenH, overlayColor);

    // Calculate panel dimensions (centered popup, ~70% of screen)
    int panelW = (int)(screenW * 0.7f);
    int panelH = (int)(screenH * 0.5f);
    int panelX = (screenW - panelW) / 2;
    int panelY = (screenH - panelH) / 2;

    // Warm parchment panel background
    DrawRectangle(panelX, panelY, panelW, panelH, COLOR_PARCHMENT);

    // Double border (clay brown outer + rust inner for depth)
    DrawRectangleLinesEx((Rectangle){panelX, panelY, panelW, panelH}, 3, COLOR_CLAY);
    DrawRectangleLinesEx((Rectangle){panelX + 8, panelY + 8, panelW - 16, panelH - 16}, 2, COLOR_RUST);

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

    // Use mixed UTF-8 sprite + font rendering with warm ink color
    DrawTextMixedUTF8(atlas, *g_ui_font, buffer, (Vector2){textX, textY}, fontSize, spacing, COLOR_INK);

    // Show prompt when text fully revealed
    bool textFullyRevealed = cutsceneState.revealedChars >= (int)strlen(panel->text);
    if (textFullyRevealed) {
        const char* prompt = "[SPACE] to continue";
        Vector2 promptSize = MeasureTextEx(*g_ui_font, prompt, 20, 2);
        int promptX = panelX + panelW - (int)promptSize.x - 20;
        int promptY = panelY + panelH - 35;
        DrawTextEx(*g_ui_font, prompt, (Vector2){promptX, promptY}, 20, 2, COLOR_INK);
    }

    // Panel counter (bottom left of panel)
    char counter[32];
    snprintf(counter, sizeof(counter), "%d / %d",
             cutsceneState.currentPanel + 1, cutsceneState.panelCount);
    DrawTextEx(*g_ui_font, counter, (Vector2){panelX + 20, panelY + panelH - 35}, 20, 2, COLOR_RUST);
}

void CloseCutscene(void) {
    cutsceneState.active = false;
}

bool IsCutsceneActive(void) {
    return cutsceneState.active;
}

void PlayTestCutscene(void) {
    InitCutscene(testPanels, 3);  // All 3 panels
}
