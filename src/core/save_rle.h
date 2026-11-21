// save_rle.h - run-length coding of per-cell layers in saves (v95+).
//
// Each layer is still one stream of gridDepth * gridHeight * gridWidth elements in [z][y][x]
// order, but coded as tokens:
//   varint (n << 1) | 1, then 1 element      : the element repeated n times (a run)
//   varint (n << 1),     then n elements     : n elements as they are (literals)
// Runs shorter than RLE_MIN_RUN go into literals, so noisy data costs only a varint per
// up-to-RLE_MAX_LITERALS elements. Terrain, sky light, ambient temperature and empty layers are
// long runs: a 2048x2048 world went from 3.65 GB to tens of MB.
// Shared by saveload.c and inspect.c (which must read the grid section the same way).
#ifndef SAVE_RLE_H
#define SAVE_RLE_H

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define RLE_MAX_ELEM      32      // largest layer element (bytes)
#define RLE_MIN_RUN       4
#define RLE_MAX_LITERALS  4096

static inline void RleWriteVarint(FILE* f, uint64_t v) {
    unsigned char b[10];
    int n = 0;
    do {
        unsigned char c = (unsigned char)(v & 0x7F);
        v >>= 7;
        if (v) c |= 0x80;
        b[n++] = c;
    } while (v);
    fwrite(b, 1, (size_t)n, f);
}

static inline bool RleReadVarint(FILE* f, uint64_t* out) {
    uint64_t v = 0;
    for (int shift = 0; shift < 64; shift += 7) {
        int c = fgetc(f);
        if (c == EOF) return false;
        v |= (uint64_t)(c & 0x7F) << shift;
        if (!(c & 0x80)) { *out = v; return true; }
    }
    return false;
}

static inline bool RleElemEqual(const unsigned char* a, const unsigned char* b, size_t es) {
    switch (es) {
        case 1: return a[0] == b[0];
        case 2: { uint16_t x, y; memcpy(&x, a, 2); memcpy(&y, b, 2); return x == y; }
        case 4: { uint32_t x, y; memcpy(&x, a, 4); memcpy(&y, b, 4); return x == y; }
        case 8: { uint64_t x, y; memcpy(&x, a, 8); memcpy(&y, b, 8); return x == y; }
        default: return memcmp(a, b, es) == 0;
    }
}

// ---- Writer ----
typedef struct {
    FILE* f;
    size_t es;
    unsigned char run[RLE_MAX_ELEM];
    uint64_t runN;
    unsigned char lit[RLE_MAX_LITERALS * RLE_MAX_ELEM];
    int litN;
} RleWriter;

static inline void RleFlushLiterals(RleWriter* w) {
    if (w->litN == 0) return;
    RleWriteVarint(w->f, (uint64_t)w->litN << 1);
    fwrite(w->lit, w->es, (size_t)w->litN, w->f);
    w->litN = 0;
}

static inline void RleFlushRun(RleWriter* w) {
    if (w->runN == 0) return;
    if (w->runN >= RLE_MIN_RUN) {
        RleFlushLiterals(w);
        RleWriteVarint(w->f, (w->runN << 1) | 1);
        fwrite(w->run, w->es, 1, w->f);
    } else {
        for (uint64_t i = 0; i < w->runN; i++) {
            if (w->litN == RLE_MAX_LITERALS) RleFlushLiterals(w);
            memcpy(w->lit + (size_t)w->litN * w->es, w->run, w->es);
            w->litN++;
        }
    }
    w->runN = 0;
}

static inline void RleWriterBegin(RleWriter* w, FILE* f, size_t es) {
    w->f = f; w->es = es; w->runN = 0; w->litN = 0;
}

static inline void RleWriterPush(RleWriter* w, const void* elems, int n) {
    const unsigned char* p = (const unsigned char*)elems;
    for (int i = 0; i < n; i++, p += w->es) {
        if (w->runN && RleElemEqual(p, w->run, w->es)) { w->runN++; continue; }
        RleFlushRun(w);
        memcpy(w->run, p, w->es);
        w->runN = 1;
    }
}

static inline void RleWriterEnd(RleWriter* w) {
    RleFlushRun(w);
    RleFlushLiterals(w);
}

// ---- Reader ----
typedef struct {
    FILE* f;
    size_t es;
    uint64_t left;          // elements left in the current token
    bool isRun;
    unsigned char run[RLE_MAX_ELEM];
    bool error;
} RleReader;

static inline void RleReaderBegin(RleReader* r, FILE* f, size_t es) {
    r->f = f; r->es = es; r->left = 0; r->isRun = false; r->error = false;
}

// Read the next n elements of the stream into dst.
static inline void RleReaderPull(RleReader* r, void* dst, uint64_t n) {
    unsigned char* d = (unsigned char*)dst;
    while (n > 0 && !r->error) {
        if (r->left == 0) {
            uint64_t v;
            if (!RleReadVarint(r->f, &v)) { r->error = true; break; }
            r->isRun = (v & 1) != 0;
            r->left = v >> 1;
            if (r->isRun && fread(r->run, r->es, 1, r->f) != 1) { r->error = true; break; }
            if (r->left == 0) continue;
        }
        uint64_t k = n < r->left ? n : r->left;
        if (r->isRun) {
            for (uint64_t i = 0; i < k; i++) memcpy(d + i * r->es, r->run, r->es);
        } else if (fread(d, r->es, (size_t)k, r->f) != (size_t)k) {
            r->error = true; break;
        }
        d += k * r->es; n -= k; r->left -= k;
    }
    if (r->error) memset(d, 0, (size_t)(n * r->es));
}

// True if the stream ended exactly where it should (no half-used token).
static inline bool RleReaderEnd(const RleReader* r) { return !r->error && r->left == 0; }

// Skip n elements (inspect.c, for layers it doesn't keep).
static inline void RleReaderSkip(RleReader* r, uint64_t n) {
    unsigned char tmp[256 * RLE_MAX_ELEM];
    while (n > 0 && !r->error) {
        uint64_t k = n < 256 ? n : 256;
        RleReaderPull(r, tmp, k);
        n -= k;
    }
}

#endif
