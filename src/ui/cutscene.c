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
Font* g_cutscene_font = NULL;

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

// Draw ASCII art sprites on a fixed grid (pass 1)
// Each cell is spriteSize x spriteSize, spaces advance by one cell
static void DrawAsciiArt(Texture2D atlasTexture, const char* text,
                         Vector2 position, int spriteSize, Color tint) {
    float x = position.x;
    float y = position.y;
    const char* p = text;

    while (*p) {
        uint32_t codepoint = DecodeUTF8(&p);
        if (codepoint == 0) break;

        if (codepoint == '\n') {
            x = position.x;
            y += spriteSize;
        } else {
            int spriteId = GetSpriteForCodepoint(codepoint);
            if (spriteId >= 0) {
                Rectangle srcRect = SPRITE8X8GetRect(spriteId);
                Rectangle destRect = { x, y, (float)spriteSize, (float)spriteSize };
                DrawTexturePro(atlasTexture, srcRect, destRect, (Vector2){0, 0}, 0, tint);
            }
            // All characters (sprite, space, or other) advance by one cell
            x += spriteSize;
        }
    }
}

// Draw font text (pass 2)
// If dropCap is true, the first printable character is drawn large with a background block.
static void DrawCutsceneText(Font font, const char* text,
                             Vector2 position, float fontSize, float spacing,
                             Color tint, bool dropCap) {
    float x = position.x;
    float y = position.y;
    float lineHeight = fontSize * 1.2f;
    const char* p = text;

    // Drop cap state
    float dropCapSize = fontSize * 1.5f;
    float dropCapRight = position.x;  // Right edge of drop cap area
    float dropCapBottom = position.y; // Bottom edge of drop cap area
    bool dropCapDrawn = false;
    int dropCapPad = 6;

    while (*p) {
        uint32_t codepoint = DecodeUTF8(&p);
        if (codepoint == 0) break;

        if (codepoint == '\n') {
            y += lineHeight;
            // If still beside the drop cap, indent; otherwise normal margin
            x = (dropCapDrawn && y < dropCapBottom) ? dropCapRight : position.x;
        } else if (codepoint == '|') {
            // Pause marker — invisible, skip rendering
            continue;
        } else if (codepoint == ' ') {
            int spaceIdx = GetGlyphIndex(font, ' ');
            if (font.glyphs && spaceIdx >= 0 && spaceIdx < font.glyphCount && font.glyphs[spaceIdx].advanceX > 0) {
                x += font.glyphs[spaceIdx].advanceX * fontSize / font.baseSize;
            } else {
                x += fontSize * 0.5f;
            }
        } else if (codepoint >= 32) {
            // Drop cap: first printable character
            if (dropCap && !dropCapDrawn) {
                dropCapDrawn = true;
                int index = GetGlyphIndex(font, codepoint);
                if (font.glyphs && index >= 0 && index < font.glyphCount) {
                    GlyphInfo glyph = font.glyphs[index];
                    float advance = (glyph.advanceX == 0) ? font.recs[index].width : glyph.advanceX;
                    float capW = (advance * dropCapSize / font.baseSize);

                    // Background block
                    DrawRectangle((int)x - dropCapPad, (int)y - dropCapPad,
                                  (int)capW + dropCapPad * 2, (int)dropCapSize + dropCapPad * 2,
                                  COLOR_INK);
                    // Letter in inverted color
                    DrawTextCodepoint(font, codepoint, (Vector2){x, y}, dropCapSize, COLOR_PARCHMENT);

                    dropCapRight = x + capW + dropCapPad * 2 + 4;
                    dropCapBottom = y + dropCapSize + dropCapPad;
                    x = dropCapRight;
                }
                continue;
            }

            int index = GetGlyphIndex(font, codepoint);
            if (font.glyphs && index >= 0 && index < font.glyphCount) {
                DrawTextCodepoint(font, codepoint, (Vector2){x, y}, fontSize, tint);
                GlyphInfo glyph = font.glyphs[index];
                float advance = (glyph.advanceX == 0) ? font.recs[index].width : glyph.advanceX;
                x += (advance * fontSize / font.baseSize) + spacing;
            }
        }
    }
}

// Hardcoded test panels (intro sequence)
static Panel testPanelsOLD[] = {
    {
        .asciiArt =
            "      ·÷·÷·\n"
            "       ▲▲▼▼▼\n"
            "   ░░▒▒▓▓█▓▓▒▒░░\n"
            " ░▒▓▓▓▓▓▓▓▓▓▓▓▓▓▓▒░",
        .text =
         "\n"
          "\n"
           "\n"
            "One awakes in the wild.|\n"
             "\n"
            "No tools, just bare hands.\n"
            "You are hungry| and cold.",
        .typewriterSpeed = 30.0f,
        .dropCap = true
    },
    {
        .asciiArt =
            "    ▄▄▄▄▄▄▄\n"
            "   █░·░░·░█\n"
            "   █░░░░░░█░░░▒▒▒▓▓▓███\n"
            "   █░░÷÷÷░░█\n"
            "   ▀▀▀▀▀▀▀",
        .text =
        "\n"
        "\n"
        "\n"
        "You are still waiting.|\n"
        "For what?| someone helping you?\n"
        "|\n"
        "YOU Help you!",
        .dropCap = true,
        .typewriterSpeed = 40.0f,
    },
    {
        .asciiArt =
                   " ·░·  ·░·  ·░·\n"
                   "  ▒▓▲░░▓▓░░▲▓▒\n"
                   "   ░▓███████▓░\n"
                   "    ·▒▓▓█▓▓▒·\n"
                   "      ·÷▼÷·",
        .text =
        "\n"
        "\n"
        "\n"
            "Gather what you can.\n"
            "What is of use.\n\n"
            "Survival begins|\n"
            "And nothing will wait.",
        .typewriterSpeed = 40.0f,
        .dropCap = true
    }
};
static Panel testPanels[] = {
    {
        .asciiArt =
            "      ·÷·÷·\n"
            "       ▲▲▼▼▼\n"
            "   ░░▒▒▓▓█▓▓▒▒░░\n"
            " ░▒▓▓▓▓▓▓▓▓▓▓▓▓▓▓▒░",
        .text =
         "\n"
          "\n"
           "\n"
            "One awakes in the wild.|\n"
             "\n"
            "No tools, just bare hands.\n"
            "You are hungry| and cold.",
        .typewriterSpeed = 30.0f,
        .dropCap = true
    },
    {
        .asciiArt =
            "    ▄▄▄▄▄▄▄\n"
            "   █░·░░·░█\n"
            "   █░░░░░░█░░░▒▒▒▓▓▓███\n"
            "   █░░÷÷÷░░█\n"
            "   ▀▀▀▀▀▀▀",
        .text =
        "\n"
        "\n"
        "\n"
        "You are still waiting.|\n"
        "For what?| someone helping you?\n"
        "|\n"
        "YOU Help you!",
        .dropCap = true,
        .typewriterSpeed = 40.0f,
    },
    {
        .asciiArt =
            " ·░·  ·░·  ·░·\n"
            "  ▒▓▲░░▓▓░░▲▓▒\n"
            "   ░▓███████▓░\n"
            "    ·▒▓▓█▓▓▒·\n"
            "      ·÷▼÷·",
        .text =
        "\n"
        "\n"
        "\n"
            "Gather what you can.\n"
            "What is of use.\n\n"
            "Survival begins|\n"
            "And nothing will wait.",
        .typewriterSpeed = 40.0f,
        .dropCap = true
    },
    {
        .asciiArt =
            "  ████▓▓▒░\n"
            "   ▓▓▒▼  ·░\n"
            "    ░·▼ ÷  ▼\n"
            "   ·    ▼\n"
            "  ÷",
        .text =
        "\n"
        "\n"
        "\n"
            "Things fall away.|\n"
            "Skin, comfort, certainty.\n\n"
            "Let them go.\n"
            "They were never yours to keep.",
        .typewriterSpeed = 35.0f,
        .dropCap = true
    },
    {
        .asciiArt =
            "  ▓██░\n"
            "  ▓█▒·▼·\n"
            "   ▒░÷·  ▼\n"
            "    ·  ÷\n"
            "     ▼  ·",
        .text =
        "\n"
        "\n"
        "\n"
            "Something is leaking.|\n"
            "Energy. Focus. Time.\n\n"
            "Patch it| or bleed out slow.",
        .typewriterSpeed = 35.0f,
        .dropCap = true
    },
    {
        .asciiArt =
            "  ÷ ·▲ ▼ ÷▲·\n"
            "   ▼░ ·░░▲\n"
            "    ▒▒▓▒░\n"
            "   ▒▓██▓▒\n"
            "   ▓████▓",
        .text =
        "\n"
        "\n"
        "\n"
            "Small things first.|\n"
            "Twigs. Stones. Scraps.\n\n"
            "They gather into something|\n"
            "heavier than they were alone.",
        .typewriterSpeed = 35.0f,
        .dropCap = true
    },
    {
        .asciiArt =
            "  ▓▓▓▓▓▓▓▓\n"
            "  ▓▓▒░÷·░▓\n"
            "  ▓░· ▲▼▲ ·▒\n"
            "  ▓▓▒▒░÷▓▓\n"
            "  ▓▓▓▓▓▓▓▓",
        .text =
        "\n"
        "\n"
        "\n"
            "There is a wound.|\n"
            "Don't look away from it.\n\n"
            "It heals| only if you let air in.",
        .typewriterSpeed = 40.0f,
        .dropCap = true
    },
    {
        .asciiArt =
            "    ▒▓▲\n"
            "  ▓█▒÷\n"
            "    ▼░▓█▒\n"
            "  ÷▒▓▲\n"
            "     ▼▒▓÷",
        .text =
        "\n"
        "\n"
        "\n"
            "The body twitches.|\n"
            "Restless. Unsure.\n\n"
            "That is not weakness.|\n"
            "That is the animal| learning.",
        .typewriterSpeed = 30.0f,
        .dropCap = true
    },
    {
        .asciiArt =
            "         ░▒▓▲\n"
            "       ░▒▓▓█\n"
            "    ▲░▒▓██\n"
            "  ·÷░▒▓█\n"
            "  ▲░▒▓",
        .text =
        "\n"
        "\n"
        "\n"
            "It grows.|\n"
            "Not because you forced it.\n\n"
            "Because you kept going|\n"
            "when stopping made more sense.",
        .typewriterSpeed = 35.0f,
        .dropCap = true
    },
    {
        .asciiArt =
            "  ▒▓██▓░\n"
            "  ░÷  ··▒▓▓\n"
            "  ▲▼▲▼▼▲▼▲\n"
            "    ÷▒▓·░",
        .text =
        "\n"
        "\n"
        "\n"
            "Hunger speaks| in a wrong mouth.|\n"
            "All teeth, no tongue.\n\n"
            "Feed it before| it feeds on you.",
        .typewriterSpeed = 40.0f,
        .dropCap = true
    },
    {
        .asciiArt =
            "  ▓▓█▒───▲─÷\n"
            "  ▒▓░\n"
            "  ░▒──▼· ▲\n"
            "  ÷░\n"
            "   ▼──÷",
        .text =
        "\n"
        "\n"
        "\n"
            "Reach.|\n"
            "Even when nothing reaches back.\n\n"
            "The hand that extends|\n"
            "is already stronger| than the one that hides.",
        .typewriterSpeed = 30.0f,
        .dropCap = true
    },
    {
        .asciiArt =
            "  ░▒▲░▓·▒▼·▓\n"
            "   ÷▓░ ▒▲█░\n"
            "  ▒·░▓▒▼ ░\n"
            "     ·▒ ░÷▓▲·",
        .text =
        "\n"
        "\n"
        "\n"
            "The noise is loud tonight.|\n"
            "Every signal scrambled.\n\n"
            "Sit still.| Wait.\n"
            "Clarity comes to the quiet.",
        .typewriterSpeed = 35.0f,
        .dropCap = true
    },
    {
        .asciiArt =
            "  ██▓▓\n"
            "  █▓▒░ ▼\n"
            "   ▒░\n"
            "   ▼÷     ░▲\n"
            "           ÷",
        .text =
        "\n"
        "\n"
        "\n"
            "Something was left behind.|\n"
            "You can see it from here.\n\n"
            "Too far to fetch.|\n"
            "You build a new one.",
        .typewriterSpeed = 35.0f,
        .dropCap = true
    },
    {
        .asciiArt =
            "  ÷      ▲\n"
            "  │   ·\n"
            "  ▒░  │\n"
            "  ▓▓▒░▒▼\n"
            "   █▓▓▒░÷·",
        .text =
        "\n"
        "\n"
        "\n"
            "You built something.|\n"
            "Ugly. Crooked. Barely standing.\n\n"
            "It receives a signal anyway.|\n"
            "That is enough.",
        .typewriterSpeed = 35.0f,
        .dropCap = true
    },
    {
        .asciiArt =
            "       ▲\n"
            "    ÷░▒\n"
            "  ▲░▒▓▓░\n"
            "  ▒▓██▓▒░÷\n"
            "  ▓███▓▒▒░▼·",
        .text =
        "\n"
        "\n"
        "\n"
            "Layer by layer.|\n"
            "Stone on stone. Day on day.\n\n"
            "You did not plan this.|\n"
            "You just| didn't stop.",
        .typewriterSpeed = 30.0f,
        .dropCap = true
    },
    {
        .asciiArt =
            "  ░▒▓██▓▒░▼\n"
            "   ░▒▓▓▒÷\n"
            "     ▼▒·\n"
            "      ÷       ▲",
        .text =
        "\n"
        "\n"
        "\n"
            "Most of it is gone now.|\n"
            "The mass. The weight.\n\n"
            "But look|— far off—|\n"
            "something still points up.",
        .typewriterSpeed = 40.0f,
        .dropCap = true
    },
};
// Count lines in a string (1 if no newlines, 0 if NULL)
static int CountLines(const char* s) {
    if (!s) return 0;
    int lines = 1;
    for (; *s; s++) {
        if (*s == '\n') lines++;
    }
    return lines;
}

void InitCutscene(const Panel* panels, int count) {
    cutsceneState.active = true;
    cutsceneState.panels = panels;
    cutsceneState.panelCount = count;
    cutsceneState.currentPanel = 0;
    cutsceneState.revealedArtLines = 0;
    cutsceneState.artLineCount = CountLines(panels[0].asciiArt);
    cutsceneState.revealedChars = 0;
    cutsceneState.timer = 0.0f;
    cutsceneState.skipTypewriter = false;
    cutsceneState.charSoundDuration = 0.0f;
}

// Find byte offset of the end of the next word from position pos in text.
// Skips leading whitespace, then advances past the word.
// '|' is a pause marker — treated as its own single-character "word".
static int NextWordEnd(const char* text, int pos) {
    int len = (int)strlen(text);
    int i = pos;
    // Skip whitespace/newlines (but include them in the reveal)
    while (i < len && (text[i] == ' ' || text[i] == '\n' || text[i] == '\t')) i++;
    // Pause marker is its own word
    if (i < len && text[i] == '|') return i + 1;
    // Advance through word characters (stop at whitespace or pause marker)
    while (i < len && text[i] != ' ' && text[i] != '\n' && text[i] != '\t' && text[i] != '|') i++;
    return i;
}

void UpdateCutscene(float dt) {
    if (!cutsceneState.active) return;

    const Panel* panel = &cutsceneState.panels[cutsceneState.currentPanel];
    int textLen = panel->text ? (int)strlen(panel->text) : 0;
    bool artDone = cutsceneState.revealedArtLines >= cutsceneState.artLineCount;
    bool textDone = cutsceneState.revealedChars >= textLen;
    bool allDone = artDone && textDone;

    if (!cutsceneState.skipTypewriter && !allDone) {
        cutsceneState.timer += dt;

        if (cutsceneState.timer >= cutsceneState.charSoundDuration) {
            if (!artDone) {
                // Reveal next ASCII art line
                cutsceneState.revealedArtLines++;
                cutsceneState.charSoundDuration = 0.06f;  // Short delay per line
            } else if (!textDone) {
                // Reveal next word
                int nextEnd = NextWordEnd(panel->text, cutsceneState.revealedChars);
                cutsceneState.revealedChars = nextEnd;

                // Check if we just revealed a pause marker '|'
                if (nextEnd > 0 && panel->text[nextEnd - 1] == '|') {
                    cutsceneState.charSoundDuration = 1.0f;  // Long dramatic pause
                } else {
                    // Play sound for the first vowel in the revealed word
                    for (int i = cutsceneState.revealedChars - 1; i >= 0 && panel->text[i] != ' ' && panel->text[i] != '\n'; i--) {
                        char c = panel->text[i];
                        if (c == 'a' || c == 'e' || c == 'i' || c == 'o' || c == 'u' ||
                            c == 'A' || c == 'E' || c == 'I' || c == 'O' || c == 'U') {
                            cutsceneState.charSoundDuration = PlayCharacterSound(c);
                            break;
                        }
                    }
                    if (cutsceneState.charSoundDuration < 0.15f) {
                        cutsceneState.charSoundDuration = 0.15f;
                    }
                }
            }
            cutsceneState.timer = 0.0f;
        }
    }

    // Input: any key or mouse click advances or skips typewriter
    if (GetKeyPressed() != 0 || IsMouseButtonPressed(MOUSE_BUTTON_LEFT) || IsMouseButtonPressed(MOUSE_BUTTON_RIGHT)) {
        if (!allDone) {
            // Skip typewriter, reveal everything
            cutsceneState.revealedArtLines = cutsceneState.artLineCount;
            cutsceneState.revealedChars = textLen;
            cutsceneState.skipTypewriter = true;
        } else {
            // Advance to next panel
            cutsceneState.currentPanel++;
            if (cutsceneState.currentPanel >= cutsceneState.panelCount) {
                CloseCutscene();
            } else {
                const Panel* next = &cutsceneState.panels[cutsceneState.currentPanel];
                cutsceneState.revealedArtLines = 0;
                cutsceneState.artLineCount = CountLines(next->asciiArt);
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

    Font cutFont = (g_cutscene_font && g_cutscene_font->texture.id > 0)
        ? *g_cutscene_font : *g_ui_font;

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

    // Content area: inside inner border + padding
    int contentX = panelX + 24;
    int contentY = panelY + 24;
    int artSpriteSize = 24;
    int fontSize = 32;
    int spacing = 2;

    // Pass 1: ASCII art (line-by-line reveal)
    int textStartY = contentY;
    if (panel->asciiArt && cutsceneState.revealedArtLines > 0) {
        // Build a buffer with only the revealed lines
        char artBuffer[2048];
        const char* src = panel->asciiArt;
        int linesIncluded = 0;
        int pos = 0;
        while (*src && linesIncluded < cutsceneState.revealedArtLines && pos < 2046) {
            if (*src == '\n') linesIncluded++;
            artBuffer[pos++] = *src++;
        }
        artBuffer[pos] = '\0';
        DrawAsciiArt(atlas, artBuffer, (Vector2){contentX, contentY}, artSpriteSize, COLOR_RUST);
    }
    // Text starts from the same origin — overlaps art (that's the point of two passes)

    // Pass 2: Font text with typewriter effect
    if (panel->text) {
        char buffer[2048];
        int len = cutsceneState.revealedChars;
        if (len > 2047) len = 2047;
        strncpy(buffer, panel->text, len);
        buffer[len] = '\0';
        DrawCutsceneText(cutFont, buffer, (Vector2){contentX, textStartY}, fontSize, spacing, COLOR_INK, panel->dropCap);
    }

    // Panel counter (bottom left of panel)
    char counter[32];
    snprintf(counter, sizeof(counter), "%d / %d",
             cutsceneState.currentPanel + 1, cutsceneState.panelCount);
    DrawTextEx(cutFont, counter, (Vector2){panelX + 20, panelY + panelH - 35}, 20, 2, COLOR_RUST);
}

void CloseCutscene(void) {
    cutsceneState.active = false;
}

bool IsCutsceneActive(void) {
    return cutsceneState.active;
}

void PlayTestCutscene(void) {
    InitCutscene(testPanels, 16);  // All 3 panels
}
