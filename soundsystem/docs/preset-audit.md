# Preset Similarity Audit

Rendered all 263 presets through `preset-audition --audit` and compared using
weighted feature-vector distance across 11 dimensions: spectral shape (32 perceptual bands),
brightness, attack, decay, noise content, inharmonicity, odd/even ratio, fundamental strength,
transient sharpness, RMS and peak levels. Lower distance = more similar.

## Near-Identical (distance < 0.08) — likely duplicates

### [54] Mel Tabla <-> [96] Tabla
- **Distance: 0.0648** (band=0.001)
- Virtually identical across all metrics
- **Action:** Remove one or differentiate

## Very Similar (distance 0.08-0.15) — worth differentiating

### [6] Chip Organ <-> [146] Chip Square
- **Distance: 0.0875** (band=0.009)
- Brightness diff: -78 Hz
- **Action:** Verify they sound different in musical context

### [2] Arp Pad <-> [9] Power Arp
- **Distance: 0.1056** (band=0.018)
- **Action:** Verify they sound different in musical context

### [11] Marimba <-> [23] Mac Vibes
- **Distance: 0.1079** (band=0.018)
- **Action:** Verify they sound different in musical context

### [194] Alto Sax <-> [196] Oboe
- **Distance: 0.1274** (band=0.037)
- Brightness diff: -51 Hz
- **Action:** Verify they sound different in musical context

### [72] Bongo Lo <-> [96] Tabla
- **Distance: 0.1283** (band=0.049)
- **Action:** Verify they sound different in musical context

### [11] Marimba <-> [47] Tubular Bell
- **Distance: 0.1326** (band=0.054)
- **Action:** Verify they sound different in musical context

### [111] Wurlitzer <-> [179] Wurli Soul
- **Distance: 0.1352** (band=0.047)
- **Action:** Verify they sound different in musical context

### [206] Trombone <-> [209] Flugelhorn
- **Distance: 0.1396** (band=0.034)
- **Action:** Verify they sound different in musical context

## Somewhat Similar (distance 0.15-0.25) — review if intended

- [23] Mac Vibes <-> [47] Tubular Bell  (dist=0.150, bright=-21Hz, attack=+0ms)
- [3] Wobble <-> [9] Power Arp  (dist=0.154, bright=+86Hz, attack=+0ms)
- [15] Piku Glock <-> [45] Glockenspiel  (dist=0.160, bright=+3Hz, attack=+0ms)
- [54] Mel Tabla <-> [72] Bongo Lo  (dist=0.163, bright=-14Hz, attack=+0ms)
- [2] Arp Pad <-> [3] Wobble  (dist=0.167, bright=-122Hz, attack=+4ms)
- [1] Fat Bass <-> [125] SNES Brass  (dist=0.171, bright=-1Hz, attack=+0ms)
- [61] Sub Bass <-> [154] MT70 Piano  (dist=0.180, bright=-0Hz, attack=+4ms)
- [192] Clarinet <-> [205] Muted Trumpet  (dist=0.180, bright=+61Hz, attack=-13ms)
- [49] Brass <-> [91] Brass Stab  (dist=0.185, bright=-4Hz, attack=+31ms)
- [111] Wurlitzer <-> [178] Wurli Buzz  (dist=0.186, bright=-8Hz, attack=+1ms)
- [127] Mono Lead <-> [163] MT70 Banjo  (dist=0.191, bright=+68Hz, attack=+3ms)
- [193] Soprano Sax <-> [195] Tenor Sax  (dist=0.191, bright=+52Hz, attack=-29ms)
- [207] French Horn <-> [209] Flugelhorn  (dist=0.194, bright=-2Hz, attack=-8ms)
- [211] Classical Gtr <-> [258] Jazz Box  (dist=0.197, bright=-30Hz, attack=+0ms)
- [178] Wurli Buzz <-> [179] Wurli Soul  (dist=0.198, bright=+30Hz, attack=-2ms)
- [204] Trumpet <-> [206] Trombone  (dist=0.206, bright=+16Hz, attack=-29ms)
- [155] MT70 Organ <-> [162] MT70 Celesta  (dist=0.207, bright=-7Hz, attack=+5ms)
- [206] Trombone <-> [207] French Horn  (dist=0.214, bright=-0Hz, attack=+4ms)
- [129] Screamer <-> [170] OBXa Brass  (dist=0.217, bright=-52Hz, attack=+1ms)
- [2] Arp Pad <-> [20] Mac Bass  (dist=0.217, bright=+50Hz, attack=-0ms)
- [127] Mono Lead <-> [181] Clav Mellow  (dist=0.223, bright=+14Hz, attack=+1ms)
- [46] Xylophone <-> [259] Grenadier Acid  (dist=0.224, bright=-24Hz, attack=-1ms)
- [19] Mac Juno <-> [92] Add Strings  (dist=0.227, bright=+109Hz, attack=-15ms)
- [205] Muted Trumpet <-> [207] French Horn  (dist=0.227, bright=+65Hz, attack=-27ms)
- [46] Xylophone <-> [88] Xylophone  (dist=0.230, bright=-8Hz, attack=+0ms)
- [194] Alto Sax <-> [207] French Horn  (dist=0.232, bright=+15Hz, attack=-33ms)
- [23] Mac Vibes <-> [259] Grenadier Acid  (dist=0.233, bright=+2Hz, attack=-1ms)
- [1] Fat Bass <-> [9] Power Arp  (dist=0.239, bright=-0Hz, attack=+0ms)
- [3] Wobble <-> [129] Screamer  (dist=0.239, bright=+52Hz, attack=+5ms)
- [196] Oboe <-> [207] French Horn  (dist=0.241, bright=+66Hz, attack=-40ms)
- [17] Piku Pluck <-> [117] Slap Bass  (dist=0.241, bright=-133Hz, attack=-0ms)
- [48] Choir <-> [50] Strings Sect  (dist=0.243, bright=-8Hz, attack=-47ms)
- [125] SNES Brass <-> [181] Clav Mellow  (dist=0.245, bright=+182Hz, attack=+4ms)

## Duplicate Name Issues

| Preset A | Preset B | Issue |
|----------|----------|-------|
| [45] Glockenspiel | [87] Glockenspiel | **Same name!** |
| [46] Xylophone | [88] Xylophone | **Same name!** |
| [52] PD Bass | [94] PD Bass | **Same name!** |
| [53] PD Lead | [95] PD Lead | **Same name!** |
| [70] Tambourine | [228] Tambourine | **Same name!** |
| [98] Guiro | [232] Guiro | **Same name!** |
| [101] Cabasa | [227] Cabasa | **Same name!** |
| [27] 808 CH | [28] 808 OH | Near-same name (1 char diff) |
| [37] Maracas | [226] Maraca | Near-same name (1 char diff) |
| [47] Tubular Bell | [89] Tubular Bells | Near-same name (1 char diff) |
| [112] Clavinet | [192] Clarinet | Near-same name (1 char diff) |
| [143] 909 CH | [144] 909 OH | Near-same name (1 char diff) |
| [198] M-808 CH | [199] M-808 OH | Near-same name (1 char diff) |

## Per-Preset Issues

### Silent (3)
- [102] Vinyl Crackle (peak=0.0096)
- [121] Grain Pad (peak=0.0000)
- [122] Grain Shimmer (peak=0.0000)

### Clipping (1)
- [167] Moog Sci-Fi (peak=1.145)

### High DC Offset (13)
- [7] Lofi Keys (DC=+0.0364)
- [38] Warm Tri Bass (DC=+0.0278)
- [41] Tri Sub (DC=+0.0502)
- [52] PD Bass (DC=-0.0486)
- [59] Flute (DC=+0.0358)
- [65] Sync Lead (DC=-0.0160)
- [79] Triangle (DC=+0.0225)
- [85] Sync Brass (DC=-0.0224)
- [120] Ocarina (DC=+0.0440)
- [132] DX7 Brass (DC=-0.0286)
- [169] Moog Sub (DC=+0.0295)
- [174] Rhodes Warm (DC=-0.0306)
- [176] Rhodes Suite (DC=-0.0183)

## Summary

- **Total presets:** 263
- **Pairs with distance < 0.25:** 42 (from 34453 total)
- **Near-identical (<0.08):** 1 pairs
- **Very similar (0.08-0.15):** 8 pairs
- **Somewhat similar (0.15-0.25):** 33 pairs
- **Silent:** 3, **Clipping:** 1, **DC offset:** 13

---

**Note on methodology:** Each preset is rendered independently, so non-deterministic synth elements
(noise generators, LFO phase, random initial conditions) add ~0.05-0.07 noise floor to distances.
Presets using `WAVE_MEMBRANE` or other stochastic engines may appear more different than they
actually are. When in doubt, listen to both presets side by side.

*Generated 2026-03-24 by `preset-audition --audit` — 263 presets rendered at MIDI 60, 1.0s note-on,
2.0s total. Feature-vector distance across 11 weighted dimensions (spectral shape, brightness, attack,
decay, noise content, inharmonicity, odd/even ratio, fundamental strength, transient sharpness, levels).*
