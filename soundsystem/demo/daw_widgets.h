// PixelSynth DAW - Bespoke Widgets
// P-lock badge, waveform thumbnails, ADSR curve, filter XY, LFO preview
// Extracted from daw.c — include after state/UI globals are defined

#ifndef PIXELSYNTH_DAW_WIDGETS_H
#define PIXELSYNTH_DAW_WIDGETS_H

// ============================================================================
// P-LOCK BADGE
// ============================================================================

static void plockDot(float x, float y) {
    DrawCircle((int)(x + 3), (int)(y + 5), 2.5f, (Color){220, 130, 50, 180});
}

// P-lockable float: draws orange dot before the control
static bool ui_col_float_p(UIColumn *c, const char *label, float *val, float speed, float mn, float mx) {
    plockDot(c->x - 8, c->y);
    return ui_col_float(c, label, val, speed, mn, mx);
}

// Section-level non-default indicator: colored left stripe + subtle bg
// active=true means at least one param in this section differs from default
static void sectionHighlight(float x, float y, float w, float h, bool active) {
    if (active) {
        DrawRectangle((int)x, (int)y, 2, (int)h, ORANGE);
    }
    (void)w;
}

// ============================================================================
// BESPOKE WIDGETS (same as daw.c)
// ============================================================================

// Helper: compute wave sample value for a given wave type and time position
static float waveThumbSample(int waveType, float t, int i) {
    float v = 0.0f;
    switch (waveType) {
        case 0: // Square
            v = t < 0.5f ? 1.0f : -1.0f;
            break;
        case 1: // Sawtooth
            v = 1.0f - 2.0f * t;
            break;
        case 2: // Triangle
            v = t < 0.5f ? (4 * t - 1) : (3 - 4 * t);
            break;
        case 3: // Noise
            v = ((float)((i * 7 + 13) % 17)) / 8.5f - 1;
            break;
        case 4: // Sine + 2nd harmonic
            v = sinf(t * 6.28f) * 0.6f + sinf(t * 12.56f) * 0.3f;
            break;
        case 5: // Sine with AM modulation
            v = sinf(t * 6.28f) * (1 - 0.5f * sinf(t * 18.84f));
            break;
        case 6: // Pluck (decay)
            v = sinf(t * 25) * expf(-t * 3);
            break;
        case 7: // Rich harmonics (3 partials)
            v = sinf(t * 6.28f) * 0.5f + sinf(t * 12.56f) * 0.3f + sinf(t * 18.84f) * 0.2f;
            break;
        case 8: // 2nd harmonic with decay
            v = sinf(t * 12.56f) * expf(-t * 3);
            break;
        case 9: // FM-like with modulation
            v = sinf(t * 31.4f) * (((int)(t * 6) % 2) ? 0.8f : 0.3f);
            break;
        case 10: // FM modulation
            v = sinf(t * 6.28f + 2 * sinf(t * 12.56f));
            break;
        case 11: // Smooth curve modulation
        {
            float p = t < 0.5f ? t * t * 2 : 0.5f + (t - 0.5f) * (2 - t * 2);
            v = sinf(p * 6.28f);
            break;
        }
        case 12: // Multiple harmonics with decay
            v = (sinf(t * 6.28f) * 0.5f + sinf(t * 9.8f) * 0.3f + sinf(t * 15.2f) * 0.2f) * expf(-t * 2);
            break;
        case 13: // Frequency sweep
            v = sinf(t * 6.28f * (1 + t * 3)) * (1 - t * 0.5f);
            break;
        case 14: // Bowed (rich harmonics)
            v = sinf(t * 6.28f) * 0.7f + sinf(t * 12.56f) * 0.2f + sinf(t * 18.84f) * 0.1f;
            break;
        case 15: // Pipe (breathy)
            v = sinf(t * 6.28f) * 0.8f + sinf(t * 18.84f) * 0.15f + ((float)((i * 3 + 7) % 11) / 55.0f);
            break;
        case 17: // EPiano (bell + bark decay)
        {
            float ep = sinf(t * 6.28f) * 0.6f + sinf(t * 12.56f) * 0.4f + sinf(t * 25.12f) * 0.15f;
            v = ep * expf(-t * 2.5f);
            break;
        }
        default: // Sine wave
            v = sinf(t * 6.28f);
            break;
    }
    return v;
}

static void drawWaveThumb(float x, float y, float w, float h, int waveType, bool selected, bool hovered) {
    Color bg = selected ? UI_BG_ACTIVE : (hovered ? UI_BG_HOVER : UI_BG_PANEL);
    DrawRectangle((int)x, (int)y, (int)w, (int)h, bg);
    if (selected) DrawRectangleLinesEx((Rectangle){x, y, w, h}, 1, ORANGE);
    else DrawRectangleLinesEx((Rectangle){x, y, w, h}, 1, UI_BORDER);

    float cx = x + 2, cy = y + 2, cw = w - 4, ch = h - 4;
    float mid = cy + ch * 0.5f;
    Color lineCol = selected ? WHITE : UI_TEXT_SUBTLE;
    int steps = (int)cw;

    for (int i = 0; i < steps - 1; i++) {
        float t0 = (float)i / (float)steps;
        float t1 = (float)(i + 1) / (float)steps;
        float v0 = waveThumbSample(waveType, t0, i);
        float v1 = waveThumbSample(waveType, t1, i + 1);
        DrawLine((int)(cx+i), (int)(mid-v0*ch*0.4f), (int)(cx+i+1), (int)(mid-v1*ch*0.4f), lineCol);
    }
}

static float drawWaveSelector(float x, float y, float w, int* wave) {
    int basicWaves[] = {WAVE_SQUARE, WAVE_SAW, WAVE_TRIANGLE, WAVE_SINE, WAVE_NOISE, WAVE_SCW};
    int waveCount = 6, engineCount1 = 5, engineCount2 = 8;
    float thumbH = 20;
    Vector2 mouse = GetMousePosition();
    float totalH = 0;

    float thumbW = (w - (waveCount - 1) * 2) / waveCount;
    for (int i = 0; i < waveCount; i++) {
        int wi = basicWaves[i];
        float tx = x + i * (thumbW + 2), ty = y;
        bool sel = (wi == *wave), hov = CheckCollisionPointRec(mouse, (Rectangle){tx, ty, thumbW, thumbH});
        drawWaveThumb(tx, ty, thumbW, thumbH, wi, sel, hov);
        if (hov && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) { *wave = wi; ui_consume_click(); }
    }
    totalH += thumbH + 2;

    DrawTextShadow("Engines:", (int)x, (int)(y + totalH), 9, (Color){70, 70, 85, 255});
    totalH += 12;

    float eW = (w - (engineCount1 - 1) * 2) / engineCount1;
    for (int i = 0; i < engineCount1; i++) {
        int wi = 5 + i;
        float tx = x + i * (eW + 2), ty = y + totalH;
        bool sel = (wi == *wave), hov = CheckCollisionPointRec(mouse, (Rectangle){tx, ty, eW, thumbH});
        drawWaveThumb(tx, ty, eW, thumbH, wi, sel, hov);
        if (hov && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) { *wave = wi; ui_consume_click(); }
    }
    totalH += thumbH + 2;

    eW = (w - (engineCount2 - 1) * 2) / engineCount2;
    for (int i = 0; i < engineCount2; i++) {
        int wi = 10 + i;
        float tx = x + i * (eW + 2), ty = y + totalH;
        bool sel = (wi == *wave), hov = CheckCollisionPointRec(mouse, (Rectangle){tx, ty, eW, thumbH});
        drawWaveThumb(tx, ty, eW, thumbH, wi, sel, hov);
        if (hov && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) { *wave = wi; ui_consume_click(); }
    }
    totalH += thumbH + 4;
    return totalH;
}

// ADSR drag state — which handle is being dragged (0=none, 1=A, 2=D, 3=S, 4=R)
static int adsrDragHandle = 0;

static float drawADSRCurve(float x, float y, float w, float h,
                            float *atk, float *dec, float *sus, float *rel, bool expRel) {
    DrawRectangle((int)x, (int)y, (int)w, (int)h, UI_BG_DEEPEST);
    DrawRectangleLinesEx((Rectangle){x, y, w, h}, 1, UI_BORDER_SUBTLE);

    float totalTime = *atk + *dec + 0.3f + *rel;
    float atkW = (*atk / totalTime) * w;
    float decW = (*dec / totalTime) * w;
    float susW = (0.3f / totalTime) * w;
    float relW = (*rel / totalTime) * w;
    float bot = y + h - 2, top = y + 2, range = bot - top;
    float susY = bot - *sus * range;
    Color cc = (Color){100, 200, 120, 255};

    // Draw envelope curve
    DrawLine((int)x, (int)bot, (int)(x+atkW), (int)top, cc);
    DrawLine((int)(x+atkW), (int)top, (int)(x+atkW+decW), (int)susY, cc);
    DrawLine((int)(x+atkW+decW), (int)susY, (int)(x+atkW+decW+susW), (int)susY, cc);
    float relStartX = x + atkW + decW + susW;
    if (expRel) {
        int steps = (int)relW;
        for (int i = 0; i < steps; i++) {
            float t0 = (float)i/steps, t1 = (float)(i+1)/steps;
            float v0 = *sus * expf(-t0*3), v1 = *sus * expf(-t1*3);
            DrawLine((int)(relStartX+i), (int)(bot-v0*range), (int)(relStartX+i+1), (int)(bot-v1*range), cc);
        }
    } else {
        DrawLine((int)relStartX, (int)susY, (int)(relStartX+relW), (int)bot, cc);
    }

    // 4 handle positions: A (peak), D (end of decay), S (sustain midpoint), R (end of release)
    Vector2 mouse = GetMousePosition();
    Color dotCol = (Color){255, 180, 60, 255};
    float atkHx = x + atkW, atkHy = top;
    float decHx = x + atkW + decW, decHy = susY;
    float susHx = x + atkW + decW + susW * 0.5f, susHy = susY;
    float relHx = relStartX + relW, relHy = bot;

    Rectangle atkDot = {atkHx - 4, atkHy - 4, 8, 8};
    Rectangle decDot = {decHx - 4, decHy - 4, 8, 8};
    Rectangle susDot = {susHx - 4, susHy - 4, 8, 8};
    Rectangle relDot = {relHx - 4, relHy - 4, 8, 8};

    bool atkHov = CheckCollisionPointRec(mouse, atkDot);
    bool decHov = CheckCollisionPointRec(mouse, decDot);
    bool susHov = CheckCollisionPointRec(mouse, susDot);
    bool relHov = CheckCollisionPointRec(mouse, relDot);

    DrawRectangleRec(atkDot, (atkHov || adsrDragHandle == 1) ? WHITE : dotCol);
    DrawRectangleRec(decDot, (decHov || adsrDragHandle == 2) ? WHITE : dotCol);
    DrawRectangleRec(susDot, (susHov || adsrDragHandle == 3) ? WHITE : dotCol);
    DrawRectangleRec(relDot, (relHov || adsrDragHandle == 4) ? WHITE : dotCol);

    // Start drag on press
    if (!g_ui_isDragging && adsrDragHandle == 0 && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        if (atkHov) adsrDragHandle = 1;
        else if (decHov) adsrDragHandle = 2;
        else if (susHov) adsrDragHandle = 3;
        else if (relHov) adsrDragHandle = 4;
    }

    // Process drag — delta-based (like DraggableFloat) so rescaling doesn't fight the handle
    if (adsrDragHandle > 0) {
        if (IsMouseButtonReleased(MOUSE_LEFT_BUTTON)) {
            adsrDragHandle = 0;
        } else {
            float dx = GetMouseDelta().x;
            float dy = GetMouseDelta().y;
            float speed = totalTime / w; // pixels to time units
            if (adsrDragHandle == 1) {
                *atk += dx * speed;
                if (*atk < 0.001f) *atk = 0.001f;
                if (*atk > 2.0f) *atk = 2.0f;
            } else if (adsrDragHandle == 2) {
                // Decay handle: X = decay time, Y = sustain level
                *dec += dx * speed;
                if (*dec < 0.001f) *dec = 0.001f;
                if (*dec > 2.0f) *dec = 2.0f;
                *sus -= dy / range;
                if (*sus < 0) *sus = 0;
                if (*sus > 1) *sus = 1;
            } else if (adsrDragHandle == 3) {
                // Sustain handle: same — X moves decay, Y moves sustain
                *dec += dx * speed;
                if (*dec < 0.001f) *dec = 0.001f;
                if (*dec > 2.0f) *dec = 2.0f;
                *sus -= dy / range;
                if (*sus < 0) *sus = 0;
                if (*sus > 1) *sus = 1;
            } else if (adsrDragHandle == 4) {
                *rel += dx * speed;
                if (*rel < 0.001f) *rel = 0.001f;
                if (*rel > 2.0f) *rel = 2.0f;
            }
        }
    }
    return h + 4;
}

static float drawFilterXY(float x, float y, float size, float *cutoff, float *resonance) {
    DrawRectangle((int)x, (int)y, (int)size, (int)size, UI_BG_DEEPEST);
    DrawRectangleLinesEx((Rectangle){x, y, size, size}, 1, UI_BORDER_SUBTLE);
    for (int i = 1; i < 4; i++) {
        float gx = x + size*i*0.25f, gy = y + size*i*0.25f;
        DrawLine((int)gx, (int)y, (int)gx, (int)(y+size), (Color){32,32,38,255});
        DrawLine((int)x, (int)gy, (int)(x+size), (int)gy, (Color){32,32,38,255});
    }
    DrawTextShadow("Cut", (int)x+2, (int)(y+size-12), 9, (Color){60,60,70,255});
    DrawTextShadow("Res", (int)(x+size-20), (int)y+2, 9, (Color){60,60,70,255});
    float cx = x + (*cutoff)*size, cy = y + (1-*resonance/1.02f)*size;
    DrawLine((int)cx, (int)y, (int)cx, (int)(y+size), (Color){60,80,100,200});
    DrawLine((int)x, (int)cy, (int)(x+size), (int)cy, (Color){60,80,100,200});
    DrawCircle((int)cx, (int)cy, 5, (Color){255,140,40,255});
    DrawCircle((int)cx, (int)cy, 3, WHITE);
    Vector2 mouse = GetMousePosition();
    if (!g_ui_isDragging && CheckCollisionPointRec(mouse, (Rectangle){x,y,size,size}) && IsMouseButtonDown(MOUSE_LEFT_BUTTON)) {
        float nc = (mouse.x-x)/size, nr = (1-(mouse.y-y)/size) * 1.02f;
        *cutoff = nc<0.01f?0.01f:(nc>1?1:nc);
        *resonance = nr<0?0:(nr>1.02f?1.02f:nr);
    }
    return size + 4;
}

static float drawLFOPreview(float x, float y, float w, float h, int shape, float rate, float depth) {
    DrawRectangle((int)x, (int)y, (int)w, (int)h, UI_BG_DEEPEST);
    DrawRectangleLinesEx((Rectangle){x, y, w, h}, 1, UI_BORDER_SUBTLE);
    if (depth < 0.001f || rate < 0.001f) {
        DrawTextShadow("off", (int)(x+w*0.5f-8), (int)(y+h*0.5f-5), 10, (Color){50,50,58,255});
        return h + 2;
    }
    float mid = y + h*0.5f, amp = h*0.4f*(depth>1?1:depth);
    float t = (float)GetTime();
    Color lc = (Color){130,130,220,255};
    for (int i = 0; i < (int)w - 1; i++) {
        float p0 = ((float)i/w)*2+t*rate*0.2f, p1 = ((float)(i+1)/w)*2+t*rate*0.2f;
        float v0=0, v1=0;
        switch (shape) {
            case 0: v0=sinf(p0*6.28f); v1=sinf(p1*6.28f); break;
            case 1: v0=fmodf(p0,1); v0=v0<0.5f?(4*v0-1):(3-4*v0); v1=fmodf(p1,1); v1=v1<0.5f?(4*v1-1):(3-4*v1); break;
            case 2: v0=fmodf(p0,1)<0.5f?1:-1; v1=fmodf(p1,1)<0.5f?1:-1; break;
            case 3: v0=1-2*fmodf(p0,1); v1=1-2*fmodf(p1,1); break;
            case 4: v0=((int)(p0*5)*7+3)%11/5.5f-1; v1=((int)(p1*5)*7+3)%11/5.5f-1; break;
        }
        DrawLine((int)(x+i),(int)(mid-v0*amp),(int)(x+i+1),(int)(mid-v1*amp),lc);
    }
    DrawLine((int)x,(int)mid,(int)(x+w),(int)mid,(Color){35,35,42,255});
    return h + 2;
}

// ============================================================================
// COMPACT LFO WIDGET — two values per row to save vertical space
// ============================================================================

// Draw two draggable floats side by side on one row (short labels)
#ifndef DAW_HEADLESS
static void ui_col_float_pair(UIColumn *c, const char *l1, float *v1, float sp1, float mn1, float mx1,
                               const char *l2, float *v2, float sp2, float mn2, float mx2) {
    int fs = c->fontSize > 0 ? c->fontSize : 12;
    float halfW = 70;  // approximate width for first control
    DraggableFloatS(c->x, c->y, l1, v1, sp1, mn1, mx1, fs);
    DraggableFloatS(c->x + halfW, c->y, l2, v2, sp2, mn2, mx2, fs);
    c->y += c->spacing;
}

// Draw a cycle + draggable float side by side on one row
static void ui_col_cycle_float_pair(UIColumn *c, const char *cl, const char **opts, int cnt, int *cv,
                                     const char *fl, float *fv, float fsp, float fmn, float fmx) {
    int fs = c->fontSize > 0 ? c->fontSize : 12;
    CycleOptionS(c->x, c->y, cl, opts, cnt, cv, fs);
    // Measure cycle text width to place float after it
    char buf[128];
    snprintf(buf, sizeof(buf), "%s: < %s >", cl, opts[*cv]);
    float cw = (float)MeasureTextUI(buf, fs) + 10;
    DraggableFloatS(c->x + cw, c->y, fl, fv, fsp, fmn, fmx, fs);
    c->y += c->spacing + 2;  // match cycle spacing
}
#endif

#endif // PIXELSYNTH_DAW_WIDGETS_H
