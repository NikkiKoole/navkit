# Preset Similarity Audit

Rendered all 146 presets through `preset-audition` and compared spectral centroid, RMS level, decay time, attack time, and crest factor. Listed below are presets that sound nearly identical or suspiciously similar.

## Near-Identical (distance < 0.01) — likely duplicates

### [54] Mel Tabla ↔ [96] Tabla
- **Distance: 0.000** — exactly identical in every metric
- Both use WAVE_MEMBRANE with the same settings
- **Action:** Remove one. Keep [96] Tabla (cleaner name), remove [54] Mel Tabla

### [121] Grain Pad ↔ [122] Grain Shimmer
- **Distance: 0.000** — both silent in 1s render (slow attack granular, no audible output)
- They likely differ when held longer, but the preset-audition can't capture that
- **Action:** Verify manually in DAW with held notes. If truly different, keep both. If not, merge.

### [0] Chip Lead ↔ [12] Piku Accord
- **Distance: 0.004** — virtually identical sound
- Both are basic square waves with similar envelopes
- **Action:** Differentiate Piku Accord (add chord arp? different filter?) or remove one

### [11] Marimba ↔ [60] Kalimba
- **Distance: 0.008** — very similar mallet sounds
- Both use WAVE_MALLET with similar settings
- **Action:** Differentiate Kalimba (shorter decay, more metallic brightness, higher pitch?) or remove one

## Very Similar (distance 0.01-0.03) — worth differentiating

### [29] 808 Tom ↔ [140] 909 Kick
- **Distance: 0.015** — similar pitch sweep + sine body
- 808 Tom is pitched higher (80Hz base) with triangle blend; 909 Kick is at 55Hz with click
- **Action:** They serve different purposes (tom vs kick) but sound close on analysis. The 909 Kick could use more differentiation — stronger click, more punch.

### [15] Piku Glock ↔ [45] Glockenspiel
- **Distance: 0.019** — same instrument, different preset series
- Both are mallet glockenspiel sounds
- **Action:** Remove [15] Piku Glock (the generic [45] Glockenspiel is better named)

### [45] Glockenspiel ↔ [87] Glockenspiel
- **Same name!** Two presets called "Glockenspiel"
- [45] uses WAVE_MALLET, [87] uses WAVE_ADDITIVE — different engines
- **Action:** Rename [87] to "Add Glockenspiel" or "Glock (Additive)" to distinguish

### [46] Xylophone ↔ [88] Xylophone
- **Same name!** Two presets called "Xylophone"
- [46] uses WAVE_MALLET, [88] uses WAVE_ADDITIVE
- **Action:** Rename [88] to "Add Xylophone" or "Xylo (Additive)"

### [47] Tubular Bell ↔ [89] Tubular Bells
- **Nearly same name!** "Tubular Bell" vs "Tubular Bells"
- [47] uses WAVE_MALLET, [89] uses WAVE_ADDITIVE
- **Action:** Rename [89] to "Add Tub Bell" or "Tubular (Additive)"

### [54] Mel Tabla / [73] Conga Hi / [74] Conga Lo / [96] Tabla
- **Cluster of similar membrane percussion** (distances 0.021-0.023 between all pairs)
- All use WAVE_MEMBRANE, only differ in trigger frequency
- **Action:** Mel Tabla is identical to Tabla (remove one). Congas are fine as a pair. Ensure Tabla sounds distinct from Conga.

### [58] Upright Bass ↔ [126] SNES Harp
- **Distance: 0.022** — both pluck-based, similar spectral profile
- Different engines (Upright = bowed init?, SNES Harp = pluck) but similar output
- **Action:** Verify they sound different when played across range. May need more tonal differentiation.

### [83] WC Lead ↔ [125] SNES Brass
- **Distance: 0.025** — West Coast lead vs SNES brass
- Similar spectral centroid and envelope
- **Action:** Minor concern — they use different synthesis techniques (wavefold vs wavetable). Likely sound different in musical context.

## Duplicate Name Issues (regardless of similarity)

| Preset A | Preset B | Issue |
|----------|----------|-------|
| [45] Glockenspiel | [87] Glockenspiel | Same name, different engine (mallet vs additive) |
| [46] Xylophone | [88] Xylophone | Same name, different engine |
| [47] Tubular Bell | [89] Tubular Bells | Near-same name, different engine |
| [52] PD Bass | [94] PD Bass | Same name! Both phase distortion bass |
| [53] PD Lead | [95] PD Lead | Same name! Both phase distortion lead |
| [48] Choir | [90] Choir Pad | Similar names, likely similar |
| [49] Brass | [91] Brass Stab | Similar names |
| [50] Strings Sect | [92] Add Strings | Similar names |
| [51] Crystal Bell | [93] Add Bell | Similar concept |

## Summary of Recommended Actions

1. **Remove** [54] Mel Tabla (exact duplicate of [96] Tabla)
2. **Remove or differentiate** [15] Piku Glock (duplicate of [45] Glockenspiel)
3. **Rename** duplicately-named presets ([87], [88], [89], [94], [95]) to include engine type
4. **Differentiate** [0] Chip Lead vs [12] Piku Accord — nearly identical square waves
5. **Differentiate** [11] Marimba vs [60] Kalimba — nearly identical mallet sounds
6. **Verify** [121] Grain Pad vs [122] Grain Shimmer with held notes in DAW
7. **Verify** [52] PD Bass vs [94] PD Bass and [53] PD Lead vs [95] PD Lead — same names, check if same params

---

*Generated 2026-03-14 by rendering all 146 presets through preset-audition and comparing 6 spectral/envelope features across 10,585 preset pairs.*
