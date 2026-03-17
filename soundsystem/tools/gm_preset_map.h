// gm_preset_map.h — General MIDI to Soundsystem preset mapping
//
// Maps GM program numbers (0-127) and GM drum notes (35-81) to
// instrumentPresets[] indices. Used by the MIDI importer.
//
// -1 = no good match, caller should use a sensible default.

#ifndef GM_PRESET_MAP_H
#define GM_PRESET_MAP_H

// General MIDI Program Number (0-127) → instrumentPresets[] index
static const int gm_program_to_preset[128] = {
    /* GM 0:   Acoustic Grand Piano  */ 16,  // Piku Piano
    /* GM 1:   Bright Acoustic Piano */ 130, // DX7 E.Piano
    /* GM 2:   Electric Grand Piano  */ 130, // DX7 E.Piano
    /* GM 3:   Honky-Tonk Piano      */ 114, // Honky Piano
    /* GM 4:   Electric Piano 1      */ 56,  // Rhodes Mel
    /* GM 5:   Electric Piano 2      */ 57,  // Rhodes Brt
    /* GM 6:   Harpsichord           */ 18,  // Mac Jangle
    /* GM 7:   Clavinet              */ 112, // Clavinet
    /* GM 8:   Celesta               */ 51,  // Crystal Bell
    /* GM 9:   Glockenspiel          */ 45,  // Glockenspiel
    /* GM 10:  Music Box             */ 15,  // Piku Glock
    /* GM 11:  Vibraphone            */ 23,  // Mac Vibes
    /* GM 12:  Marimba               */ 11,  // Marimba
    /* GM 13:  Xylophone             */ 46,  // Xylophone
    /* GM 14:  Tubular Bells         */ 47,  // Tubular Bell
    /* GM 15:  Dulcimer              */ 60,  // Kalimba
    /* GM 16:  Drawbar Organ         */ 6,   // Chip Organ
    /* GM 17:  Percussive Organ      */ 106, // FM Organ
    /* GM 18:  Rock Organ            */ 39,  // Dark Organ
    /* GM 19:  Church Organ          */ 39,  // Dark Organ
    /* GM 20:  Reed Organ            */ 106, // FM Organ
    /* GM 21:  Accordion             */ 119, // Accordion
    /* GM 22:  Harmonica             */ 119, // Accordion
    /* GM 23:  Tango Accordion       */ 119, // Accordion
    /* GM 24:  Nylon Guitar          */ 62,  // Nylon Guitar
    /* GM 25:  Steel Guitar          */ 18,  // Mac Jangle
    /* GM 26:  Jazz Guitar           */ 62,  // Nylon Guitar
    /* GM 27:  Clean Guitar          */ 44,  // Warm Pluck
    /* GM 28:  Muted Guitar          */ 118, // Mute Guitar
    /* GM 29:  Overdrive Guitar      */ 63,  // Wavefold Lead
    /* GM 30:  Distortion Guitar     */ 129, // Screamer
    /* GM 31:  Guitar Harmonics      */ 51,  // Crystal Bell
    /* GM 32:  Acoustic Bass         */ 58,  // Upright Bass
    /* GM 33:  Finger Bass           */ 20,  // Mac Bass
    /* GM 34:  Pick Bass             */ 1,   // Fat Bass
    /* GM 35:  Fretless Bass         */ 115, // Fretless Bass
    /* GM 36:  Slap Bass 1           */ 117, // Slap Bass
    /* GM 37:  Slap Bass 2           */ 117, // Slap Bass
    /* GM 38:  Synth Bass 1          */ 52,  // PD Bass
    /* GM 39:  Synth Bass 2          */ 61,  // Sub Bass
    /* GM 40:  Violin                */ 108, // Bowed Fiddle
    /* GM 41:  Viola                 */ 107, // Bowed Cello
    /* GM 42:  Cello                 */ 107, // Bowed Cello
    /* GM 43:  Contrabass            */ 58,  // Upright Bass
    /* GM 44:  Tremolo Strings       */ 43,  // Lush Strings
    /* GM 45:  Pizzicato Strings     */ 17,  // Piku Pluck
    /* GM 46:  Harp                  */ 126, // SNES Harp
    /* GM 47:  Timpani               */ 29,  // 808 Tom
    /* GM 48:  String Ensemble 1     */ 5,   // Strings
    /* GM 49:  String Ensemble 2     */ 50,  // Strings Sect
    /* GM 50:  Synth Strings 1       */ 124, // SNES Strings
    /* GM 51:  Synth Strings 2       */ 43,  // Lush Strings
    /* GM 52:  Choir Aahs            */ 48,  // Choir
    /* GM 53:  Voice Oohs            */ 22,  // Mac Vox
    /* GM 54:  Synth Choir           */ 42,  // Dark Choir
    /* GM 55:  Orchestra Hit         */ 91,  // Brass Stab
    /* GM 56:  Trumpet               */ 49,  // Brass
    /* GM 57:  Trombone              */ 49,  // Brass
    /* GM 58:  Tuba                  */ 13,  // Piku Tuba
    /* GM 59:  Muted Trumpet         */ 125, // SNES Brass
    /* GM 60:  French Horn           */ 49,  // Brass
    /* GM 61:  Brass Section         */ 132, // DX7 Brass
    /* GM 62:  Synth Brass 1         */ 85,  // Sync Brass
    /* GM 63:  Synth Brass 2         */ 125, // SNES Brass
    /* GM 64:  Soprano Sax           */ 53,  // PD Lead
    /* GM 65:  Alto Sax              */ 53,  // PD Lead
    /* GM 66:  Tenor Sax             */ 83,  // WC Lead
    /* GM 67:  Baritone Sax          */ 81,  // WC Bass
    /* GM 68:  Oboe                  */ 59,  // Flute
    /* GM 69:  English Horn          */ 109, // Pipe Flute
    /* GM 70:  Bassoon               */ 13,  // Piku Tuba
    /* GM 71:  Clarinet              */ 59,  // Flute
    /* GM 72:  Piccolo               */ 136, // FM Flute
    /* GM 73:  Flute                 */ 59,  // Flute
    /* GM 74:  Recorder              */ 110, // Recorder
    /* GM 75:  Pan Flute             */ 109, // Pipe Flute
    /* GM 76:  Blown Bottle          */ 109, // Pipe Flute
    /* GM 77:  Shakuhachi            */ 59,  // Flute
    /* GM 78:  Whistle               */ 120, // Ocarina
    /* GM 79:  Ocarina               */ 120, // Ocarina
    /* GM 80:  Square Lead           */ 146, // Chip Square
    /* GM 81:  Saw Lead              */ 147, // Chip Saw
    /* GM 82:  Calliope Lead         */ 0,   // Chip Lead
    /* GM 83:  Chiff Lead            */ 65,  // Sync Lead
    /* GM 84:  Charang Lead          */ 63,  // Wavefold Lead
    /* GM 85:  Voice Lead            */ 40,  // Eerie Vowel
    /* GM 86:  Fifths Lead           */ 127, // Mono Lead
    /* GM 87:  Bass + Lead           */ 128, // Hoover
    /* GM 88:  New Age Pad           */ 139, // Glass Pad
    /* GM 89:  Warm Pad              */ 138, // Warm Pad
    /* GM 90:  Polysynth Pad         */ 137, // Supersaw Pad
    /* GM 91:  Choir Pad             */ 90,  // Choir Pad
    /* GM 92:  Bowed Pad             */ 82,  // WC Pad
    /* GM 93:  Metallic Pad          */ 86,  // Ring Drone
    /* GM 94:  Halo Pad              */ 121, // Grain Pad
    /* GM 95:  Sweep Pad             */ 122, // Grain Shimmer
    /* GM 96:  Rain (FX)             */ 122, // Grain Shimmer
    /* GM 97:  Soundtrack (FX)       */ 123, // Dark Drone
    /* GM 98:  Crystal (FX)          */ 103, // FM Crystal
    /* GM 99:  Atmosphere (FX)       */ 121, // Grain Pad
    /* GM 100: Brightness (FX)       */ 104, // FM Bright EP
    /* GM 101: Goblins (FX)          */ 40,  // Eerie Vowel
    /* GM 102: Echoes (FX)           */ 2,   // Arp Pad
    /* GM 103: Sci-Fi (FX)           */ 3,   // Wobble
    /* GM 104: Sitar                 */ 64,  // Ring Bell
    /* GM 105: Banjo                 */ 4,   // 8bit Pluck
    /* GM 106: Shamisen              */ 10,  // Pluck
    /* GM 107: Koto                  */ 17,  // Piku Pluck
    /* GM 108: Kalimba               */ 60,  // Kalimba
    /* GM 109: Bagpipe               */ 86,  // Ring Drone
    /* GM 110: Fiddle                */ 108, // Bowed Fiddle
    /* GM 111: Shanai                */ 129, // Screamer
    /* GM 112: Tinkle Bell           */ 14,  // Piku Bell
    /* GM 113: Agogo                 */ 77,  // Agogo Hi
    /* GM 114: Steel Drums           */ 135, // FM Marimba
    /* GM 115: Woodblock             */ 76,  // Woodblock
    /* GM 116: Taiko Drum            */ 24,  // 808 Kick
    /* GM 117: Melodic Tom           */ 29,  // 808 Tom
    /* GM 118: Synth Drum            */ 84,  // WC Perc
    /* GM 119: Reverse Cymbal        */ 68,  // Crash
    /* GM 120: Guitar Fret Noise     */ -1,  // no good match
    /* GM 121: Breath Noise          */ -1,  // no good match
    /* GM 122: Seashore              */ 102, // Vinyl Crackle
    /* GM 123: Bird Tweet            */ 97,  // Bird Song
    /* GM 124: Telephone Ring        */ 133, // DX7 Bell
    /* GM 125: Helicopter            */ -1,  // no good match
    /* GM 126: Applause              */ -1,  // no good match
    /* GM 127: Gunshot               */ -1,  // no good match
};

// General MIDI Drum Note (35-81) → instrumentPresets[] index
// Usage: gm_drum_to_preset[midi_note - 35]
static const int gm_drum_to_preset[47] = {
    /* 35: Acoustic Bass Drum  */ 140, // 909 Kick
    /* 36: Bass Drum 1         */ 24,  // 808 Kick
    /* 37: Side Stick          */ 99,  // Cross Stick
    /* 38: Acoustic Snare      */ 25,  // 808 Snare
    /* 39: Hand Clap           */ 26,  // 808 Clap
    /* 40: Electric Snare      */ 141, // 909 Snare
    /* 41: Low Floor Tom       */ 29,  // 808 Tom
    /* 42: Closed Hi-Hat       */ 27,  // 808 CH
    /* 43: High Floor Tom      */ 29,  // 808 Tom
    /* 44: Pedal Hi-Hat        */ 143, // 909 CH
    /* 45: Low Tom             */ 29,  // 808 Tom
    /* 46: Open Hi-Hat         */ 28,  // 808 OH
    /* 47: Low-Mid Tom         */ 29,  // 808 Tom
    /* 48: Hi-Mid Tom          */ 29,  // 808 Tom
    /* 49: Crash Cymbal 1      */ 68,  // Crash
    /* 50: High Tom            */ 29,  // 808 Tom
    /* 51: Ride Cymbal 1       */ 66,  // Ride Cymbal
    /* 52: Chinese Cymbal      */ 68,  // Crash
    /* 53: Ride Bell           */ 66,  // Ride Cymbal
    /* 54: Tambourine          */ 70,  // Tambourine
    /* 55: Splash Cymbal       */ 68,  // Crash
    /* 56: Cowbell             */ 31,  // Cowbell
    /* 57: Crash Cymbal 2      */ 68,  // Crash
    /* 58: Vibraslap           */ -1,  // no good match
    /* 59: Ride Cymbal 2       */ 66,  // Ride Cymbal
    /* 60: Hi Bongo            */ 71,  // Bongo Hi
    /* 61: Low Bongo           */ 72,  // Bongo Lo
    /* 62: Mute Hi Conga       */ 73,  // Conga Hi
    /* 63: Open Hi Conga       */ 73,  // Conga Hi
    /* 64: Low Conga           */ 74,  // Conga Lo
    /* 65: High Timbale        */ 75,  // Timbales
    /* 66: Low Timbale         */ 75,  // Timbales
    /* 67: High Agogo          */ 77,  // Agogo Hi
    /* 68: Low Agogo           */ 78,  // Agogo Lo
    /* 69: Cabasa              */ 101, // Cabasa
    /* 70: Maracas             */ 37,  // Maracas
    /* 71: Short Whistle       */ -1,  // no good match
    /* 72: Long Whistle        */ -1,  // no good match
    /* 73: Short Guiro         */ 98,  // Guiro
    /* 74: Long Guiro          */ 98,  // Guiro
    /* 75: Claves              */ 36,  // Clave
    /* 76: Hi Woodblock        */ 76,  // Woodblock
    /* 77: Low Woodblock       */ 76,  // Woodblock
    /* 78: Mute Cuica          */ -1,  // no good match
    /* 79: Open Cuica          */ -1,  // no good match
    /* 80: Mute Triangle       */ 79,  // Triangle
    /* 81: Open Triangle       */ 79,  // Triangle
};

// Helper: look up preset for a GM drum note, returns -1 if out of range
static inline int gmDrumPreset(int midiNote) {
    if (midiNote < 35 || midiNote > 81) return -1;
    return gm_drum_to_preset[midiNote - 35];
}

// Helper: look up preset for a GM program, returns -1 if out of range
static inline int gmProgramPreset(int program) {
    if (program < 0 || program > 127) return -1;
    return gm_program_to_preset[program];
}

#endif // GM_PRESET_MAP_H
