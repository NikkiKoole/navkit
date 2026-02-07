// Texture Atlas Generator
// Scans assets/textures16x16/ and assets/textures8x8/ for PNG files and packs them into atlases
// Outputs: assets/atlas16x16.png/h, assets/atlas8x8.png/h, and assets/atlas.h (selector)

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include "vendor/raylib.h"

#define MAX_SPRITES 256
#define ATLAS_PADDING 1

typedef struct {
    char name[64];      // Sprite name (filename without extension)
    Image image;
    int atlasX, atlasY; // Position in atlas
} SpriteEntry;

// Convert filename to valid C identifier (replace - with _)
static void sanitize_name(const char *filename, char *out) {
    const char *dot = strrchr(filename, '.');
    int len = dot ? (int)(dot - filename) : (int)strlen(filename);
    for (int i = 0; i < len && i < 63; i++) {
        char c = filename[i];
        out[i] = (c == '-' || c == ' ') ? '_' : c;
    }
    out[len < 63 ? len : 63] = '\0';
}

// Generate atlas from a texture directory
// Returns number of sprites on success, -1 on error
// If outNames is not NULL, fills it with sprite names (caller must provide array of MAX_SPRITES char[64])
static int generate_atlas(const char *textureDir, const char *outputPng,
                          const char *outputHeader, const char *atlasPathMacro,
                          const char *headerGuard, const char *spritePrefix,
                          char outNames[][64]) {
    SpriteEntry sprites[MAX_SPRITES];
    int spriteCount = 0;

    printf("\n=== Generating atlas from %s ===\n", textureDir);

    // Scan directory for PNG files
    DIR *dir = opendir(textureDir);
    if (!dir) {
        fprintf(stderr, "Error: Cannot open directory %s\n", textureDir);
        return -1;
    }

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL && spriteCount < MAX_SPRITES) {
        const char *name = entry->d_name;
        size_t len = strlen(name);

        // Skip non-PNG files and any atlas files
        if (len < 5) continue;
        if (strcmp(name + len - 4, ".png") != 0) continue;
        if (strncmp(name, "atlas", 5) == 0) continue;

        // Load image
        char path[256];
        snprintf(path, sizeof(path), "%s/%s", textureDir, name);

        Image img = LoadImage(path);
        if (!IsImageValid(img)) {
            fprintf(stderr, "Warning: Failed to load %s\n", path);
            continue;
        }

        SpriteEntry *sprite = &sprites[spriteCount++];
        sprite->image = img;
        sanitize_name(name, sprite->name);

        printf("Loaded: %s (%dx%d)\n", name, img.width, img.height);
    }
    closedir(dir);

    if (spriteCount == 0) {
        fprintf(stderr, "Warning: No PNG files found in %s, skipping\n", textureDir);
        return 0;
    }

    // Calculate atlas dimensions (simple row packing)
    int atlasWidth = 0;
    int atlasHeight = 0;
    int currentX = 0;
    int rowHeight = 0;
    int maxRowWidth = 512; // Max width before wrapping to new row

    for (int i = 0; i < spriteCount; i++) {
        Image *img = &sprites[i].image;

        // Check if we need to wrap to next row
        if (currentX + img->width > maxRowWidth && currentX > 0) {
            atlasHeight += rowHeight + ATLAS_PADDING;
            currentX = 0;
            rowHeight = 0;
        }

        sprites[i].atlasX = currentX;
        sprites[i].atlasY = atlasHeight;

        currentX += img->width + ATLAS_PADDING;
        if (img->height > rowHeight) rowHeight = img->height;
        if (currentX > atlasWidth) atlasWidth = currentX;
    }
    atlasHeight += rowHeight;

    printf("Atlas size: %dx%d\n", atlasWidth, atlasHeight);

    // Create atlas image (transparent background)
    Image atlas = GenImageColor(atlasWidth, atlasHeight, BLANK);

    // Draw all sprites onto atlas
    for (int i = 0; i < spriteCount; i++) {
        SpriteEntry *sprite = &sprites[i];
        Rectangle srcRec = { 0, 0, sprite->image.width, sprite->image.height };
        Rectangle dstRec = { sprite->atlasX, sprite->atlasY, sprite->image.width, sprite->image.height };
        ImageDraw(&atlas, sprite->image, srcRec, dstRec, WHITE);
    }

    // Export atlas PNG
    if (ExportImage(atlas, outputPng)) {
        printf("Exported: %s\n", outputPng);
    } else {
        fprintf(stderr, "Error: Failed to export %s\n", outputPng);
    }

    // Generate header file
    FILE *header = fopen(outputHeader, "w");
    if (!header) {
        fprintf(stderr, "Error: Cannot create %s\n", outputHeader);
        return -1;
    }

    fprintf(header, "// Auto-generated texture atlas header\n");
    fprintf(header, "// Do not edit manually - regenerate with: make atlas\n\n");
    fprintf(header, "#ifndef %s\n", headerGuard);
    fprintf(header, "#define %s\n\n", headerGuard);
    fprintf(header, "#include \"vendor/raylib.h\"\n\n");
    fprintf(header, "#define %s \"%s\"\n\n", atlasPathMacro, outputPng);

    // Embed the PNG file as a byte array
    FILE *pngFile = fopen(outputPng, "rb");
    if (pngFile) {
        fseek(pngFile, 0, SEEK_END);
        long pngSize = ftell(pngFile);
        fseek(pngFile, 0, SEEK_SET);

        unsigned char *pngData = malloc(pngSize);
        if (pngData) {
            fread(pngData, 1, pngSize, pngFile);

            // Write size constant
            fprintf(header, "#define %s_DATA_SIZE %ld\n\n", atlasPathMacro, pngSize);

            // Write byte array
            fprintf(header, "static const unsigned char %s_DATA[%ld] = {\n", atlasPathMacro, pngSize);
            for (long i = 0; i < pngSize; i++) {
                if (i % 16 == 0) fprintf(header, "    ");
                fprintf(header, "0x%02x", pngData[i]);
                if (i < pngSize - 1) fprintf(header, ",");
                if (i % 16 == 15 || i == pngSize - 1) fprintf(header, "\n");
                else fprintf(header, " ");
            }
            fprintf(header, "};\n\n");

            // Write helper function to load texture from embedded data
            fprintf(header, "// Load texture from embedded PNG data\n");
            fprintf(header, "static inline Texture2D %sLoadEmbedded(void) {\n", spritePrefix);
            fprintf(header, "    Image img = LoadImageFromMemory(\".png\", %s_DATA, %s_DATA_SIZE);\n", atlasPathMacro, atlasPathMacro);
            fprintf(header, "    Texture2D tex = LoadTextureFromImage(img);\n");
            fprintf(header, "    UnloadImage(img);\n");
            fprintf(header, "    return tex;\n");
            fprintf(header, "}\n\n");

            free(pngData);
            printf("Embedded %ld bytes of PNG data\n", pngSize);
        }
        fclose(pngFile);
    }

    fprintf(header, "typedef struct {\n");
    fprintf(header, "    const char *name;\n");
    fprintf(header, "    Rectangle rect;  // x, y, width, height in atlas\n");
    fprintf(header, "} %sSprite;\n\n", spritePrefix);

    fprintf(header, "enum {\n");
    for (int i = 0; i < spriteCount; i++) {
        fprintf(header, "    %s_%s,\n", spritePrefix, sprites[i].name);
    }
    fprintf(header, "    %s_COUNT\n", spritePrefix);
    fprintf(header, "};\n\n");

    fprintf(header, "static const %sSprite %s_SPRITES[%s_COUNT] = {\n", spritePrefix, spritePrefix, spritePrefix);
    for (int i = 0; i < spriteCount; i++) {
        SpriteEntry *s = &sprites[i];
        fprintf(header, "    { \"%s\", { %d, %d, %d, %d } },\n",
                s->name, s->atlasX, s->atlasY, s->image.width, s->image.height);
    }
    fprintf(header, "};\n\n");

    fprintf(header, "// Helper: get sprite rectangle by enum\n");
    fprintf(header, "static inline Rectangle %sGetRect(int spriteId) {\n", spritePrefix);
    fprintf(header, "    return %s_SPRITES[spriteId].rect;\n", spritePrefix);
    fprintf(header, "}\n\n");

    fprintf(header, "#endif // %s\n", headerGuard);
    fclose(header);

    printf("Exported: %s\n", outputHeader);

    // Copy sprite names to output if requested
    if (outNames) {
        for (int i = 0; i < spriteCount; i++) {
            strncpy(outNames[i], sprites[i].name, 63);
            outNames[i][63] = '\0';
        }
    }

    // Cleanup
    UnloadImage(atlas);
    for (int i = 0; i < spriteCount; i++) {
        UnloadImage(sprites[i].image);
    }

    printf("Done! %d sprites packed into atlas.\n", spriteCount);
    return spriteCount;
}

// Check if a sprite name exists in an array of names
static bool sprite_exists(const char *name, char names[][64], int count) {
    for (int i = 0; i < count; i++) {
        if (strcmp(name, names[i]) == 0) return true;
    }
    return false;
}

// Generate the unified atlas.h selector header
static int generate_selector_header(const char *outputPath, char names[][64], int nameCount) {
    FILE *f = fopen(outputPath, "w");
    if (!f) {
        fprintf(stderr, "Error: Cannot create %s\n", outputPath);
        return 1;
    }

    fprintf(f, "// Atlas selector - choose tile size at compile time\n");
    fprintf(f, "// Auto-generated - do not edit manually, regenerate with: make atlas\n");
    fprintf(f, "// Usage: compile with -DTILE_SIZE=8 or -DTILE_SIZE=16 (default: 16)\n\n");
    fprintf(f, "#ifndef ATLAS_H\n");
    fprintf(f, "#define ATLAS_H\n\n");
    fprintf(f, "#ifndef TILE_SIZE\n");
    fprintf(f, "#define TILE_SIZE 8\n");
    fprintf(f, "#endif\n\n");

    fprintf(f, "#if TILE_SIZE == 8\n");
    fprintf(f, "    #include \"atlas8x8.h\"\n");
    fprintf(f, "    #define ATLAS_PATH ATLAS8X8_PATH\n");
    fprintf(f, "    #define ATLAS_DATA ATLAS8X8_PATH_DATA\n");
    fprintf(f, "    #define ATLAS_DATA_SIZE ATLAS8X8_PATH_DATA_SIZE\n");
    fprintf(f, "    #define AtlasLoadEmbedded SPRITE8X8LoadEmbedded\n");
    for (int i = 0; i < nameCount; i++) {
        fprintf(f, "    #define SPRITE_%s SPRITE8X8_%s\n", names[i], names[i]);
    }
    fprintf(f, "    #define SpriteGetRect SPRITE8X8GetRect\n");

    fprintf(f, "#elif TILE_SIZE == 16\n");
    fprintf(f, "    #include \"atlas16x16.h\"\n");
    fprintf(f, "    #define ATLAS_PATH ATLAS16X16_PATH\n");
    fprintf(f, "    #define ATLAS_DATA ATLAS16X16_PATH_DATA\n");
    fprintf(f, "    #define ATLAS_DATA_SIZE ATLAS16X16_PATH_DATA_SIZE\n");
    fprintf(f, "    #define AtlasLoadEmbedded SPRITE16X16LoadEmbedded\n");
    for (int i = 0; i < nameCount; i++) {
        fprintf(f, "    #define SPRITE_%s SPRITE16X16_%s\n", names[i], names[i]);
    }
    fprintf(f, "    #define SpriteGetRect SPRITE16X16GetRect\n");

    fprintf(f, "#else\n");
    fprintf(f, "    #error \"TILE_SIZE must be 8 or 16\"\n");
    fprintf(f, "#endif\n\n");
    fprintf(f, "#endif // ATLAS_H\n");

    fclose(f);
    printf("\nExported: %s\n", outputPath);
    return 0;
}

// Configuration (passed from Makefile, defaults to 0)
#ifndef GENERATE_16X16
#define GENERATE_16X16 0
#endif

int main(void) {
    int result = 0;

    static char spriteNames[MAX_SPRITES][64];
    int spriteCount = 0;

    // Generate 16x16 atlas (optional)
    if (GENERATE_16X16) {
        spriteCount = generate_atlas(
            "assets/textures16x16",
            "assets/atlas16x16.png",
            "assets/atlas16x16.h",
            "ATLAS16X16_PATH",
            "ATLAS16X16_H",
            "SPRITE16X16",
            spriteNames
        );
        if (spriteCount < 0) result = 1;
    }

    // Generate 8x8 atlas
    static char spriteNames8[MAX_SPRITES][64];
    int count8 = generate_atlas(
        "assets/textures8x8",
        "assets/atlas8x8.png",
        "assets/atlas8x8.h",
        "ATLAS8X8_PATH",
        "ATLAS8X8_H",
        "SPRITE8X8",
        spriteNames8
    );
    if (count8 < 0) result = 1;

    // Validate both atlases have matching sprites (only when both are generated)
    if (GENERATE_16X16 && spriteCount > 0 && count8 > 0) {
        if (spriteCount != count8) {
            fprintf(stderr, "\nERROR: Sprite count mismatch! 16x16 has %d sprites, 8x8 has %d sprites\n", spriteCount, count8);
            result = 1;
        } else {
            for (int i = 0; i < spriteCount; i++) {
                if (!sprite_exists(spriteNames[i], spriteNames8, count8)) {
                    fprintf(stderr, "\nERROR: Sprite '%s' exists in 16x16 but not in 8x8\n", spriteNames[i]);
                    result = 1;
                }
            }
            for (int i = 0; i < count8; i++) {
                if (!sprite_exists(spriteNames8[i], spriteNames, spriteCount)) {
                    fprintf(stderr, "\nERROR: Sprite '%s' exists in 8x8 but not in 16x16\n", spriteNames8[i]);
                    result = 1;
                }
            }
            if (result == 0) {
                printf("\nSprite check passed: both atlases have the same %d sprites\n", spriteCount);
            }
        }
    }

    // Generate unified selector header (use 8x8 names when 16x16 is disabled)
    char (*names)[64] = GENERATE_16X16 ? spriteNames : spriteNames8;
    int nameCount = GENERATE_16X16 ? spriteCount : count8;
    if (nameCount > 0 && result == 0) {
        result |= generate_selector_header("assets/atlas.h", names, nameCount);
    }

    return result;
}
