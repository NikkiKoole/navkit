# DAW UI Audit — Style Inventory & Inconsistencies

> Status: HISTORICAL SNAPSHOT (may be out of date)

## Files Examined
- `shared/ui.h` — widget library (DraggableFloat, ToggleBool, CycleOption, PushButton, UIColumn)
- `soundsystem/demo/daw.c` — main UI rendering (~5600 lines)
- `soundsystem/demo/daw_widgets.h` — bespoke custom widgets (ADSR, filter XY, LFO, waveform, p-lock badge)

---

## 1. Font Size System (DONE)

Two constants in `shared/ui.h` control all text-based widget sizes:

```c
#define UI_FONT_MEDIUM 14   // Default for all widgets
#define UI_FONT_SMALL  12   // Compact variant (patch settings, dense panels)
```

### What uses what

| Size | Where | How |
|------|-------|-----|
| `UI_FONT_MEDIUM` (14) | All `DraggableFloat/Int`, `ToggleBool`, `PushButton`, `CycleOption` (and tooltip `T` variants) | Hardwired in the non-S functions |
| `UI_FONT_MEDIUM` (14) | Bus FX panel (`drawParamBus`) | `int fs = UI_FONT_MEDIUM` |
| `UI_FONT_MEDIUM` (14) | P-Lock row (step inspector) | `int plFS = UI_FONT_MEDIUM` |
| `UI_FONT_MEDIUM` (14) | Groove section labels (Nudge, Timing, Track Swing, Melody) | Direct `DrawTextShadow(..., UI_FONT_MEDIUM, ...)` |
| `UI_FONT_MEDIUM` (14) | `ui_col_label` default, `ui_column()` default | Falls through to non-S functions |
| `UI_FONT_SMALL` (12) | Patch settings (all synth params) | `PCOL` macro: `ui_column_small(px, py, UI_FONT_SMALL+1, UI_FONT_SMALL)` |
| `UI_FONT_SMALL` (12) | `ui_col_sublabel` default | `col->fontSize - 2` or `UI_FONT_SMALL` |
| `UI_FONT_SMALL` (12) | Bus FX sub-controls (filter cut/res/type beside XY pad) | `int sfs = fs - 2` |

### Sized variants (`S` suffix)

All widgets have an `S` variant that takes an explicit `fontSize` parameter. The non-S versions now just use `UI_FONT_MEDIUM` internally. To change all widget text globally, edit the two `#define`s.

### Still hardcoded (not yet unified)

| Size | Where |
|------|-------|
| 18pt | Title "PixelSynth" |
| 9pt | Groove preset buttons, bus header names, sampler voice count, CPU meter, various small labels |
| 10pt | Bus header names, scene labels, sampler header labels |
| 11pt | Transport bar buttons, tab labels, sidebar patch slots |
| 13pt | Step inspector header ("Step N - TrackName") |
| 8pt | Clip indicator, step numbers (32-step mode) |

These are intentionally different sizes for hierarchy — not widget text.

---

## 2. Color Theme (Background Palette)

The dark background palette is fairly coherent across the app:

| Role | Color Range | Examples |
|------|-------------|----------|
| Deepest background | `{20-24, 20-25, 25-30, 255}` | Grid workspace, volume bar bg, CPU meter bg |
| Panel/sidebar bg | `{25-28, 25-29, 30-35, 255}` | Sidebar, transport bar, tab bar |
| Button normal | `{32-36, 33-38, 40-46, 255}` | Most inactive buttons, scene bg |
| Button hover | `{42-50, 44-50, 52-60, 255}` | Hover states across the board |
| Borders | `{48-55, 48-55, 58-65, 255}` | Button outlines, panel dividers |

**All elements use sharp corners** — no `DrawRectangleRounded` anywhere.

---

## 3. Accent Color System

| Color | Semantic Meaning | Where Used |
|-------|-----------------|------------|
| **ORANGE** `{255,161,0}` | Active / special / selected | Tab underlines, p-lock dots, filter XY indicator, section stripes, preset picker selection, sample slice borders |
| **GREEN** `{0,228,48}` | Playing / enabled / positive | Play button, active patterns, song mode on, volume fill bars, generate button |
| **RED** `{230,41,55}` | Recording / danger / destructive | Record button, mute indicators, clear button hover, clip indicator |
| **Gold** `{170,150-160,50-80}` | Master / crossfader / groove | Master volume bar fill, crossfade text, groove label |
| **Pale blue** `{130-180, 130-200, 200-255}` | Info / cool accent | LFO waveform, section sublabels, quantize mode, euclidean params |

---

## 4. Text Colors

| Role | Color(s) Used | Notes |
|------|--------------|-------|
| Primary / active | `WHITE` | Consistent |
| Hover text | `LIGHTGRAY {200,200,200}` | Consistent |
| Inactive / disabled | `GRAY {130,130,130}` | Consistent |
| Muted labels | `{70,70,85}` | Sidebar labels |
| Parameter labels | `{140,140,160}` | Patch settings, groove labels |
| Section headers | `{180,200,255}` | Pale blue |
| Sublabels | `{140,160,200}` | PWM/Formant/Wavetable |
| Gold accent text | `{170,160,80}` | Master vol label |
| Groove label | `{255,200,120}` | Warm orange |

**Inconsistency:** "secondary/muted" text uses at least 4 different grays: `GRAY`, `{70,70,80}`, `{70,70,85}`, `{140,140,160}`.

---

## 5. Button & Toggle Patterns

### Pattern A: Tab-Style (underline indicator)

Used by: **tab bar, piano roll track tabs, groove presets**

```
Active:  colored bg tint + 2px colored underline at bottom + WHITE text
Hover:   mid-dark bg + LIGHTGRAY text
Normal:  bar bg color + GRAY text
```

Underline color = ORANGE (tabs), track color (piano roll), ORANGE (groove).

### Pattern B: Button-Style (background + border color shift)

Used by: **play, record, debug, fill, 16/32 toggle, song mode, pattern lock, copy, clear, mute**

```
Active:  tinted bg + colored border + WHITE text
Hover:   lighter bg + subtle border
Normal:  dark bg {32-36, 33-38, 40-46} + dark border {48-55, 48-58, 58-65}
```

**Problem:** Each button has its own hand-tuned active tint:
- Play active: `{40, 80, 40}` (green)
- Song mode active: `{45, 80, 45}` (green, slightly different)
- Pattern active: `{50, 90, 50}` (green, different again)
- Fill active: `{140, 80, 50}` (orange)
- Debug open: `{80, 60, 30}` (brown)
- Lock active: `{50, 50, 60}` (neutral, uses text color instead)
- Mute active: `{140, 50, 50}` (red)
- Clear hover: `{60, 40, 40}` (red tint)

### Pattern C: P-Lock Badge

Used by: **parameter lock indicators only**

```
Orange circle, radius 2.5, at (x+3, y+5)
Color: {220, 130, 50, 180} (orange, semi-transparent)
```

### Pattern D: Section Active Stripe

Used by: **patch settings sections (filter, LFO, etc.)**

```
2px wide ORANGE bar on left edge when section has non-default values
No background color change
```

### Pattern E: Step/Pattern Indicator Dots

Used by: **drum step position, pattern content indicator**

```
Orange circle {255, 180, 60}, radius 3
Drawn above/beside the element
```

### Pattern F: Record Button (unique, complex)

Three states with blinking animation:
```
Recording:  red bg blinking at 4Hz + WHITE text + red circle icon
Armed:      dark red bg blinking at 2Hz + pale red text + dim circle
Off:        dark bg + muted red text + dark red circle
```

---

## 6. P-Lock Row (DONE)

The step inspector P-Lock controls now use `UI_FONT_MEDIUM` for both labels and values. Layout is computed dynamically:

```c
int plFS = UI_FONT_MEDIUM;
int plBoxH = plFS + 6;                              // box height
int plLblW = MeasureTextUI("Tone:", plFS) + 4;      // widest label (drum) / "FEnv:" (melody)
int plBoxW = MeasureTextUI("+00.0", plFS) + 8;      // widest value
int plStep = plLblW + plBoxW + 8;                   // spacing per control
```

Previously: labels 10pt, values 9pt, box height 14px, hardcoded pixel offsets.

---

## 7. Bespoke Widgets (in `daw_widgets.h`)

These are the well-factored custom widgets with their own drawing functions:

| Widget | Function | Colors |
|--------|----------|--------|
| P-Lock badge | `DrawPLockBadge` | Orange circle |
| Section highlight | `DrawSectionHighlight` | Orange left stripe |
| Waveform thumbnail | `DrawWaveformThumbnail` | White/muted waveform on dark bg, orange border when selected |
| ADSR curve | `DrawADSRCurve` | Green line `{100,200,120}`, orange dots `{255,180,60}` |
| Filter XY pad | `DrawFilterXY` | Grid lines, blue crosshairs, orange dot, white inner dot |
| LFO preview | `DrawLFOPreview` | Blue waveform `{130,130,220}`, subtle center line |

All share: dark background `{22,22,28}`, border `{42,42,52}`.

---

## 8. Track / Bus Color System

### Bus colors (8 tracks):
```
Kick:     {220, 60, 60}    — red
Snare:    {230, 140, 50}   — orange
CH:       {220, 200, 50}   — yellow
Clap:     {180, 80, 200}   — purple
Bass:     {60, 100, 220}   — blue
Lead:     {50, 200, 200}   — cyan
Chord:    {60, 200, 80}    — green
Sampler:  {200, 120, 60}   — warm orange
```

### Piano roll track colors (subset, slightly different values):
```
Bass:     {80, 140, 220}   — blue (vs bus {60,100,220})
Lead:     {220, 140, 80}   — orange (vs bus {50,200,200} cyan!)
Chord:    {140, 200, 100}  — green (vs bus {60,200,80})
Sampler:  {200, 120, 60}   — matches bus
```

**Inconsistency:** Lead is cyan in bus colors but orange in piano roll. Bass/chord values differ between the two arrays.

### Sampler slice colors (8, with 220 alpha):
```
{220,80,80}, {80,180,220}, {220,180,60}, {100,220,100},
{200,100,220}, {220,140,60}, {60,200,180}, {180,180,220}
```

---

## 9. Specific Hover/Border Inconsistencies

### Hover background (should be one value, currently ~5):
- `{42, 44, 52}` — pattern buttons, waveform thumbnails
- `{45, 45, 55}` — play button, mute hover, debug hover
- `{45, 48, 55}` — rhythm generator buttons
- `{48, 50, 60}` — song arrangement hover
- `{50, 50, 60}` — song mode inactive hover, groove preset hover
- `{50, 54, 64}` — scene hover

### Normal button background (should be one value, currently ~4):
- `{32, 33, 40}` — play, mute
- `{33, 33, 40}` — record, debug, lock, quantize, rec mode
- `{33, 34, 40}` — 16/32, fill, pattern, rhythm buttons
- `{36, 38, 46}` — groove presets, scenes

### Default border (should be one value, currently ~3):
- `{48, 48, 58}` — most buttons, patterns, euclidean
- `{50, 50, 60}` — sampler volume
- `{55, 55, 65}` — record, lock, rec mode, quantize

---

## 10. Popup / Overlay Patterns

### Preset picker popup:
```
Background:  {25, 25, 35, 245} (semi-transparent)
Border:      {80, 80, 100} (1px)
Selected:    {50, 50, 80} bg + ORANGE text
Hover:       {40, 40, 60} bg + WHITE text
Normal:      no bg + {160, 160, 180} text
```

### Text edit (song name):
```
Background:  {25, 25, 35}
Border:      {100, 150, 220} (blue)
Cursor:      WHITE, blinking at 3Hz
```

---

## 11. What's Been Done

### Font size unification
- `UI_FONT_MEDIUM` / `UI_FONT_SMALL` constants in `shared/ui.h`
- All 11 widget functions (non-S, S, T variants) use `UI_FONT_MEDIUM` instead of hardcoded 18
- `ui_col_label`/`ui_col_sublabel` defaults use the constants
- Bus FX panel uses `UI_FONT_MEDIUM`
- Patch settings PCOL uses `UI_FONT_SMALL`
- P-Lock row uses `UI_FONT_MEDIUM` with `MeasureTextUI`-based dynamic layout
- Groove section (Nudge, Timing, Track Swing, Melody) labels use `UI_FONT_MEDIUM`, spacing computed from font

### Layout fixes
- Step inspector Y position fixed for 8 tracks (was 7, missed sampler row)
- Step inspector params offset nudged right (`x+130` → `x+160`)
- Groove "TrkSw:" renamed to "Track Swing:" with proper label width

---

## 12. What's Left to Unify

### High Impact (define as constants, use everywhere)

1. **Button backgrounds** — pick ONE normal, ONE hover value
2. **Border color** — pick ONE default border color
3. **Secondary text color** — pick ONE muted gray for inactive labels
4. **Active tint variants** — define green-active, red-active, orange-active, neutral-active as constants

### Medium Impact (extract shared functions)

5. **Standard button/toggle function** with states (normal/hover/active/disabled) and color variants (green/red/orange/neutral)
6. **Tab-style function** with underline color parameter
7. **Remaining hardcoded font sizes** — transport bar (11pt), sidebar (11pt), rhythm generator (10pt), etc. could reference constants

### Low Impact (cosmetic alignment)

8. **Piano roll track colors vs bus colors** — should these match?
9. **Active indicator style** — decide rules for when to use underline vs background vs dot vs stripe
10. **Record button** — complex enough to be its own widget in `daw_widgets.h`
