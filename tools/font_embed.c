// Font Embedder
// Converts .fnt + .png font files into a C header with embedded data
// Usage: ./font_embed <font.fnt> <output.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void write_byte_array(FILE *out, const char *name, const unsigned char *data, long size) {
    fprintf(out, "#define %s_SIZE %ld\n\n", name, size);
    fprintf(out, "static const unsigned char %s[%ld] = {\n", name, size);
    for (long i = 0; i < size; i++) {
        if (i % 16 == 0) fprintf(out, "    ");
        fprintf(out, "0x%02x", data[i]);
        if (i < size - 1) fprintf(out, ",");
        if (i % 16 == 15 || i == size - 1) fprintf(out, "\n");
        else fprintf(out, " ");
    }
    fprintf(out, "};\n\n");
}

static unsigned char *read_file(const char *path, long *size) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    
    fseek(f, 0, SEEK_END);
    *size = ftell(f);
    fseek(f, 0, SEEK_SET);
    
    unsigned char *data = malloc(*size);
    if (data) {
        fread(data, 1, *size, f);
    }
    fclose(f);
    return data;
}

int main(int argc, char *argv[]) {
    if (argc < 3) {
        fprintf(stderr, "Usage: %s <font.fnt> <output.h> [PREFIX] [FUNC_NAME]\n", argv[0]);
        return 1;
    }

    const char *fntPath = argv[1];
    const char *outputPath = argv[2];
    const char *prefix = (argc > 3) ? argv[3] : "EMBEDDED_FONT";
    const char *funcName = (argc > 4) ? argv[4] : "LoadEmbeddedFont";
    
    // Derive PNG path from FNT path (same name, .png extension)
    char pngPath[256];
    strncpy(pngPath, fntPath, sizeof(pngPath) - 1);
    char *dot = strrchr(pngPath, '.');
    if (dot) {
        strcpy(dot, ".png");
    } else {
        strcat(pngPath, ".png");
    }
    
    // Read FNT file
    long fntSize = 0;
    unsigned char *fntData = read_file(fntPath, &fntSize);
    if (!fntData) {
        fprintf(stderr, "Error: Cannot read %s\n", fntPath);
        return 1;
    }
    printf("Read %s: %ld bytes\n", fntPath, fntSize);
    
    // Read PNG file
    long pngSize = 0;
    unsigned char *pngData = read_file(pngPath, &pngSize);
    if (!pngData) {
        fprintf(stderr, "Error: Cannot read %s\n", pngPath);
        free(fntData);
        return 1;
    }
    printf("Read %s: %ld bytes\n", pngPath, pngSize);
    
    // Write header
    FILE *out = fopen(outputPath, "w");
    if (!out) {
        fprintf(stderr, "Error: Cannot create %s\n", outputPath);
        free(fntData);
        free(pngData);
        return 1;
    }
    
    fprintf(out, "// Auto-generated embedded font data\n");
    fprintf(out, "// Do not edit manually - regenerate with: make embed_font\n\n");
    fprintf(out, "#ifndef %s_H\n", prefix);
    fprintf(out, "#define %s_H\n\n", prefix);
    fprintf(out, "#include \"vendor/raylib.h\"\n");
    fprintf(out, "#include <string.h>\n");
    fprintf(out, "#include <stdlib.h>\n");
    fprintf(out, "#include <stdio.h>\n\n");

    // Add null terminator to FNT data so strstr works correctly
    unsigned char *fntDataNullTerm = malloc(fntSize + 1);
    memcpy(fntDataNullTerm, fntData, fntSize);
    fntDataNullTerm[fntSize] = '\0';

    char fntArrayName[256], pngArrayName[256];
    snprintf(fntArrayName, sizeof(fntArrayName), "%s_FNT", prefix);
    snprintf(pngArrayName, sizeof(pngArrayName), "%s_PNG", prefix);

    write_byte_array(out, fntArrayName, fntDataNullTerm, fntSize + 1);
    write_byte_array(out, pngArrayName, pngData, pngSize);
    
    free(fntDataNullTerm);
    
    // Write helper function to load font from embedded data
    fprintf(out, "// Load font from embedded data\n");
    fprintf(out, "static inline Font %s(void) {\n", funcName);
    fprintf(out, "    // Load the font texture from embedded PNG\n");
    fprintf(out, "    Image img = LoadImageFromMemory(\".png\", %s_PNG, %s_PNG_SIZE);\n", prefix, prefix);
    fprintf(out, "    Texture2D texture = LoadTextureFromImage(img);\n");
    fprintf(out, "    UnloadImage(img);\n");
    fprintf(out, "    \n");
    fprintf(out, "    // Parse the .fnt file to get font info\n");
    fprintf(out, "    // Note: This is a simplified loader - works with BMFont format\n");
    fprintf(out, "    Font font = {0};\n");
    fprintf(out, "    font.texture = texture;\n");
    fprintf(out, "    \n");
    fprintf(out, "    // Parse FNT data (text format)\n");
    fprintf(out, "    const char *fntText = (const char *)%s_FNT;\n", prefix);
    fprintf(out, "    \n");
    fprintf(out, "    // Count glyphs first\n");
    fprintf(out, "    int glyphCount = 0;\n");
    fprintf(out, "    const char *p = fntText;\n");
    fprintf(out, "    while ((p = strstr(p, \"char id=\")) != NULL) {\n");
    fprintf(out, "        glyphCount++;\n");
    fprintf(out, "        p++;\n");
    fprintf(out, "    }\n");
    fprintf(out, "    \n");
    fprintf(out, "    font.glyphCount = glyphCount;\n");
    fprintf(out, "    font.glyphs = (GlyphInfo *)RL_CALLOC(glyphCount, sizeof(GlyphInfo));\n");
    fprintf(out, "    font.recs = (Rectangle *)RL_CALLOC(glyphCount, sizeof(Rectangle));\n");
    fprintf(out, "    \n");
    fprintf(out, "    // Parse common line for lineHeight and base\n");
    fprintf(out, "    p = strstr(fntText, \"common \");\n");
    fprintf(out, "    if (p) {\n");
    fprintf(out, "        int lineHeight = 0, base = 0;\n");
    fprintf(out, "        sscanf(p, \"common lineHeight=%%d base=%%d\", &lineHeight, &base);\n");
    fprintf(out, "        font.baseSize = lineHeight;\n");
    fprintf(out, "    }\n");
    fprintf(out, "    \n");
    fprintf(out, "    // Parse each char line\n");
    fprintf(out, "    p = fntText;\n");
    fprintf(out, "    int i = 0;\n");
    fprintf(out, "    while ((p = strstr(p, \"char id=\")) != NULL && i < glyphCount) {\n");
    fprintf(out, "        int id, x, y, width, height, xoffset, yoffset, xadvance;\n");
    fprintf(out, "        if (sscanf(p, \"char id=%%d x=%%d y=%%d width=%%d height=%%d xoffset=%%d yoffset=%%d xadvance=%%d\",\n");
    fprintf(out, "                   &id, &x, &y, &width, &height, &xoffset, &yoffset, &xadvance) >= 8) {\n");
    fprintf(out, "            font.glyphs[i].value = id;\n");
    fprintf(out, "            font.glyphs[i].offsetX = xoffset;\n");
    fprintf(out, "            font.glyphs[i].offsetY = yoffset;\n");
    fprintf(out, "            font.glyphs[i].advanceX = xadvance;\n");
    fprintf(out, "            font.recs[i] = (Rectangle){ (float)x, (float)y, (float)width, (float)height };\n");
    fprintf(out, "            i++;\n");
    fprintf(out, "        }\n");
    fprintf(out, "        p++;\n");
    fprintf(out, "    }\n");
    fprintf(out, "    \n");
    fprintf(out, "    return font;\n");
    fprintf(out, "}\n\n");
    
    fprintf(out, "#endif // %s_H\n", prefix);
    
    fclose(out);
    free(fntData);
    free(pngData);
    
    printf("Generated %s (total embedded: %ld bytes)\n", outputPath, fntSize + pngSize);
    return 0;
}
