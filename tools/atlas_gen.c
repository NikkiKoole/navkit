// Texture Atlas Generator
// Scans assets/textures/ for PNG files and packs them into a single atlas
// Outputs: assets/textures/atlas.png and assets/textures/atlas.h

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

int main(void) {
    const char *textureDir = "assets/textures";
    const char *outputPng = "assets/atlas.png";
    const char *outputHeader = "assets/atlas.h";

    SpriteEntry sprites[MAX_SPRITES];
    int spriteCount = 0;

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
        
        // Skip non-PNG files and the atlas itself
        if (len < 5) continue;
        if (strcmp(name + len - 4, ".png") != 0) continue;
        if (strcmp(name, "atlas.png") == 0) continue;

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
        fprintf(stderr, "Error: No PNG files found in %s\n", textureDir);
        return 1;
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

    printf("\nAtlas size: %dx%d\n", atlasWidth, atlasHeight);

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
    fprintf(header, "#ifndef ATLAS_H\n");
    fprintf(header, "#define ATLAS_H\n\n");
    fprintf(header, "#include \"vendor/raylib.h\"\n\n");
    fprintf(header, "#define ATLAS_PATH \"assets/atlas.png\"\n\n");
    
    fprintf(header, "typedef struct {\n");
    fprintf(header, "    const char *name;\n");
    fprintf(header, "    Rectangle rect;  // x, y, width, height in atlas\n");
    fprintf(header, "} AtlasSprite;\n\n");

    fprintf(header, "enum {\n");
    for (int i = 0; i < spriteCount; i++) {
        fprintf(header, "    SPRITE_%s,\n", sprites[i].name);
    }
    fprintf(header, "    SPRITE_COUNT\n");
    fprintf(header, "};\n\n");

    fprintf(header, "static const AtlasSprite ATLAS_SPRITES[SPRITE_COUNT] = {\n");
    for (int i = 0; i < spriteCount; i++) {
        SpriteEntry *s = &sprites[i];
        fprintf(header, "    { \"%s\", { %d, %d, %d, %d } },\n",
                s->name, s->atlasX, s->atlasY, s->image.width, s->image.height);
    }
    fprintf(header, "};\n\n");

    fprintf(header, "// Helper: get sprite rectangle by enum\n");
    fprintf(header, "static inline Rectangle AtlasGetRect(int spriteId) {\n");
    fprintf(header, "    return ATLAS_SPRITES[spriteId].rect;\n");
    fprintf(header, "}\n\n");

    fprintf(header, "#endif // ATLAS_H\n");
    fclose(header);

    printf("Exported: %s\n", outputHeader);

    // Cleanup
    UnloadImage(atlas);
    for (int i = 0; i < spriteCount; i++) {
        UnloadImage(sprites[i].image);
    }

    printf("\nDone! %d sprites packed into atlas.\n", spriteCount);
    return 0;
}
