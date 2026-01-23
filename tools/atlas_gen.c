// Texture Atlas Generator
// Scans assets/textures/ and assets/textures8x8/ for PNG files and packs them into atlases
// Outputs: assets/atlas16x16.png, assets/atlas16x16.h, assets/atlas8x8.png, assets/atlas8x8.h

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
// Returns 0 on success, 1 on error
static int generate_atlas(const char *textureDir, const char *outputPng, 
                          const char *outputHeader, const char *atlasPathMacro,
                          const char *headerGuard, const char *spritePrefix) {
    SpriteEntry sprites[MAX_SPRITES];
    int spriteCount = 0;

    printf("\n=== Generating atlas from %s ===\n", textureDir);

    // Scan directory for PNG files
    DIR *dir = opendir(textureDir);
    if (!dir) {
        fprintf(stderr, "Error: Cannot open directory %s\n", textureDir);
        return 1;
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
        return 0;  // Not an error, just skip
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
        return 1;
    }

    fprintf(header, "// Auto-generated texture atlas header\n");
    fprintf(header, "// Do not edit manually - regenerate with: make atlas\n\n");
    fprintf(header, "#ifndef %s\n", headerGuard);
    fprintf(header, "#define %s\n\n", headerGuard);
    fprintf(header, "#include \"vendor/raylib.h\"\n\n");
    fprintf(header, "#define %s \"%s\"\n\n", atlasPathMacro, outputPng);
    
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

    // Cleanup
    UnloadImage(atlas);
    for (int i = 0; i < spriteCount; i++) {
        UnloadImage(sprites[i].image);
    }

    printf("Done! %d sprites packed into atlas.\n", spriteCount);
    return 0;
}

int main(void) {
    int result = 0;

    // Generate main atlas (16x16 textures)
    result |= generate_atlas(
        "assets/textures",
        "assets/atlas16x16.png",
        "assets/atlas16x16.h",
        "ATLAS16X16_PATH",
        "ATLAS16X16_H",
        "SPRITE16X16"
    );

    // Generate 8x8 atlas
    result |= generate_atlas(
        "assets/textures8x8",
        "assets/atlas8x8.png",
        "assets/atlas8x8.h",
        "ATLAS8X8_PATH",
        "ATLAS8X8_H",
        "SPRITE8X8"
    );

    return result;
}
